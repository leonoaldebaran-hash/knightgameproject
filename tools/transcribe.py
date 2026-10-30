"""Transforma músicas gravadas em notas, para servir de base aos arranjos do jogo.

1. Demucs separa a faixa em voz, baixo, outros instrumentos e bateria.
2. Basic Pitch converte cada parte (menos a bateria) em notas MIDI.
3. As notas são alinhadas às batidas e resumidas compasso a compasso em
   docs/transcricao/<faixa>.md: melodia (nota mais aguda), baixo (mais grave) e acorde provável.

Uso: python tools/transcribe.py after_future [outra_faixa ...]
"""
import os
import subprocess
import sys

import librosa
import numpy as np
from basic_pitch import ICASSP_2022_MODEL_PATH
from basic_pitch.inference import predict

SRC = "assets/music_src"
SEP = "build/separated"
MIDI_OUT = "assets/midi_src"
DOC_OUT = "docs/transcricao"
STEMS = ["vocals", "other", "bass"]
NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
MAX_BARS = 64
STEPS_PER_BEAT = 4   # semicolcheias


def note_name(midi):
    return f"{NAMES[midi % 12]}{midi // 12 - 1}"


def chord_name(pitch_classes):
    """Acorde mais provável (maior ou menor) a partir do peso de cada nota no compasso."""
    best, best_score = "-", 0.0
    for root in range(12):
        for quality, third in (("", 4), ("m", 3)):
            score = pitch_classes[root] + pitch_classes[(root + third) % 12] + pitch_classes[(root + 7) % 12]
            if score > best_score:
                best, best_score = NAMES[root] + quality, score
    return best


def grid_line(notes, step_times, pick):
    """Uma nota por semicolcheia: a mais aguda (melodia) ou a mais grave (baixo)."""
    cells = []
    for t in step_times:
        sounding = [n for n in notes if n[0] <= t < n[1]]
        if not sounding:
            cells.append(None)
        else:
            cells.append(pick(n[2] for n in sounding))
    return cells


def compress(cells):
    """Junta notas repetidas: 'C5 . . .' vira 'C5(4)'. '_' é pausa."""
    out, i = [], 0
    while i < len(cells):
        j = i
        while j < len(cells) and cells[j] == cells[i]:
            j += 1
        label = "_" if cells[i] is None else note_name(cells[i])
        out.append(label if j - i == 1 else f"{label}({j - i})")
        i = j
    return " ".join(out)


def transcribe(track):
    mp3 = os.path.join(SRC, f"{track}.mp3")
    print(f"== {track}: separando instrumentos", flush=True)
    subprocess.run([sys.executable, "-m", "demucs", "-n", "htdemucs", "-o", SEP, mp3], check=True)

    y, sr = librosa.load(mp3, sr=22050, mono=True)
    tempo, beats = librosa.beat.beat_track(y=y, sr=sr, units="time")
    tempo = float(np.atleast_1d(tempo)[0])
    beat_len = 60.0 / tempo
    start = float(beats[0]) if len(beats) else 0.0

    os.makedirs(MIDI_OUT, exist_ok=True)
    stem_notes = {}
    for stem in STEMS:
        wav = os.path.join(SEP, "htdemucs", track, f"{stem}.wav")
        print(f"== {track}: notas de {stem}", flush=True)
        _, midi, events = predict(wav, ICASSP_2022_MODEL_PATH)
        midi.write(os.path.join(MIDI_OUT, f"{track}_{stem}.mid"))
        # (início, fim, nota MIDI, força); descarta notas muito fracas
        stem_notes[stem] = [(e[0], e[1], int(e[2]), float(e[3])) for e in events if e[3] > 0.35]

    lines = [
        f"# Transcrição: {track}",
        "",
        f"Andamento estimado: **{tempo:.0f} BPM** · início da primeira batida: {start:.2f} s",
        "",
        "Gerado por `tools/transcribe.py` (Demucs + Basic Pitch). É uma extração automática e aproximada:",
        "serve como base para os arranjos, não como partitura exata.",
        "",
        "Formato: uma célula por semicolcheia, `C5(4)` = C5 durante 4 semicolcheias, `_` = pausa.",
        "",
    ]
    for bar in range(MAX_BARS):
        bar_start = start + bar * 4 * beat_len
        steps = [bar_start + (k + 0.5) * beat_len / STEPS_PER_BEAT for k in range(4 * STEPS_PER_BEAT)]
        if steps[-1] > len(y) / sr:
            break

        melody_src = stem_notes["vocals"] if any(bar_start <= n[0] < bar_start + 4 * beat_len for n in stem_notes["vocals"]) else stem_notes["other"]
        melody = grid_line(melody_src, steps, max)
        bass = grid_line(stem_notes["bass"], steps, min)

        pcs = np.zeros(12)
        for n in stem_notes["other"] + stem_notes["bass"]:
            overlap = max(0.0, min(n[1], bar_start + 4 * beat_len) - max(n[0], bar_start))
            pcs[n[2] % 12] += overlap * n[3]

        lines.append(f"**Compasso {bar + 1}** · acorde: {chord_name(pcs)}")
        lines.append(f"- melodia: `{compress(melody)}`")
        lines.append(f"- baixo: `{compress(bass)}`")
        lines.append("")

    os.makedirs(DOC_OUT, exist_ok=True)
    with open(os.path.join(DOC_OUT, f"{track}.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"== {track}: pronto", flush=True)


if __name__ == "__main__":
    for name in sys.argv[1:]:
        transcribe(name)
