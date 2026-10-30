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


MAJOR = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
MINOR = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])


def estimate_key(notes):
    weights = np.zeros(12)
    for start, end, midi, _ in notes:
        weights[midi % 12] += end - start
    best = (-2.0, "")
    for i in range(12):
        for profile, mode in ((MAJOR, "maior"), (MINOR, "menor")):
            corr = np.corrcoef(np.roll(profile, i), weights)[0, 1]
            if corr > best[0]:
                best = (corr, f"{NAMES[i]} {mode}")
    return best[1]


def transcribe_voice(path):
    """Melodia cantada ou cantarolada: uma voz só, então não precisa separar instrumentos."""
    name = os.path.splitext(os.path.basename(path))[0]
    print(f"== {name}: notas da voz", flush=True)
    _, midi, events = predict(path, ICASSP_2022_MODEL_PATH)
    os.makedirs(MIDI_OUT, exist_ok=True)
    midi.write(os.path.join(MIDI_OUT, f"{name}.mid"))

    # Notas firmes e não muito curtas, uma de cada vez (a voz é uma linha só)
    notes = sorted((e[0], e[1], int(e[2]), float(e[3])) for e in events if e[3] > 0.3 and e[1] - e[0] > 0.07)
    mono = []
    for n in notes:
        if mono and n[0] < mono[-1][1]:
            # Sobreposição: fica a nota mais forte
            if n[3] > mono[-1][3]:
                mono[-1] = (mono[-1][0], n[0], mono[-1][2], mono[-1][3])
                if mono[-1][1] - mono[-1][0] < 0.07:
                    mono.pop()
                mono.append(n)
            continue
        mono.append(n)

    y, sr = librosa.load(path, sr=22050, mono=True)
    tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
    tempo = float(np.atleast_1d(tempo)[0])
    onsets = np.diff([n[0] for n in mono])
    pulse = float(np.median(onsets)) if len(onsets) else 0.0

    lines = [
        f"# Transcrição da voz: {name}",
        "",
        f"Arquivo: `{path}` · duração {len(y) / sr:.0f} s · {len(mono)} notas",
        f"Tom provável: **{estimate_key(mono)}** · andamento medido: ~{tempo:.0f} BPM · "
        f"intervalo mais comum entre notas: {pulse:.2f} s",
        "",
        "Gerado por `tools/transcribe.py` (Basic Pitch). Frases separadas por pausas de mais de 0,6 s.",
        "Formato: `nota(duração em segundos)`, e `·0.4` é uma pausa curta dentro da frase.",
        "",
    ]
    phrase, phrase_no = [], 1
    for i, (start, end, note, _) in enumerate(mono):
        gap = start - mono[i - 1][1] if i else 0.0
        if phrase and gap > 0.6:
            lines.append(f"**Frase {phrase_no}** ({phrase[0][0]:.1f} s): " + " ".join(p[1] for p in phrase))
            lines.append("")
            phrase, phrase_no = [], phrase_no + 1
        token = f"{note_name(note)}({end - start:.2f})"
        if phrase and gap > 0.12:
            token = f"·{gap:.1f} " + token
        phrase.append((start, token))
    if phrase:
        lines.append(f"**Frase {phrase_no}** ({phrase[0][0]:.1f} s): " + " ".join(p[1] for p in phrase))

    os.makedirs(DOC_OUT, exist_ok=True)
    with open(os.path.join(DOC_OUT, f"{name}.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"== {name}: pronto", flush=True)


if __name__ == "__main__":
    for name in sys.argv[1:]:
        # Um caminho de arquivo é uma gravação de voz; um nome é uma faixa de assets/music_src
        if os.path.isfile(name):
            transcribe_voice(name)
        else:
            transcribe(name)
