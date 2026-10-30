"""Mede as características de cada música em assets/music_src e gera docs/MUSICA_ANALISE.md.

Não substitui ouvir a música: são números (andamento, tom, energia, brilho) que ajudam
a decidir em que lugar do jogo cada faixa combina.
"""
import glob
import os

import librosa
import numpy as np

SRC = "assets/music_src"
OUT = "docs/MUSICA_ANALISE.md"
NOTES = ["Dó", "Dó#", "Ré", "Ré#", "Mi", "Fá", "Fá#", "Sol", "Sol#", "Lá", "Lá#", "Si"]

# Perfis de Krumhansl para estimar o tom
MAJOR = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
MINOR = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])
BARS = "▁▂▃▄▅▆▇█"


def estimate_key(chroma_mean):
    best = (-2.0, "", "")
    for i in range(12):
        for profile, mode in ((MAJOR, "maior"), (MINOR, "menor")):
            corr = np.corrcoef(np.roll(profile, i), chroma_mean)[0, 1]
            if corr > best[0]:
                best = (corr, NOTES[i], mode)
    return best[1], best[2]


def sparkline(values):
    lo, hi = min(values), max(values)
    span = (hi - lo) or 1.0
    return "".join(BARS[int((v - lo) / span * (len(BARS) - 1))] for v in values)


def analyze(path):
    y, sr = librosa.load(path, sr=22050, mono=True)
    duration = len(y) / sr
    tempo, _ = librosa.beat.beat_track(y=y, sr=sr)
    tempo = float(np.atleast_1d(tempo)[0])
    chroma = librosa.feature.chroma_cqt(y=y, sr=sr)
    note, mode = estimate_key(chroma.mean(axis=1))
    rms = librosa.feature.rms(y=y)[0]
    rms_db = librosa.amplitude_to_db(rms + 1e-9)
    centroid = float(librosa.feature.spectral_centroid(y=y, sr=sr)[0].mean())
    onsets = librosa.onset.onset_detect(y=y, sr=sr)
    harmonic, percussive = librosa.effects.hpss(y)
    perc_ratio = float(np.sum(percussive ** 2) / (np.sum(harmonic ** 2) + 1e-9))

    # Energia ao longo da música em 16 trechos
    chunks = np.array_split(rms, 16)
    curve = [float(np.mean(c)) for c in chunks]

    return {
        "name": os.path.splitext(os.path.basename(path))[0],
        "duration": duration,
        "tempo": tempo,
        "key": f"{note} {mode}",
        "mode": mode,
        "loudness": float(np.mean(rms_db)),
        "dynamics": float(np.percentile(rms_db, 95) - np.percentile(rms_db, 10)),
        "brightness": centroid,
        "density": len(onsets) / duration,
        "percussive": perc_ratio,
        "curve": curve,
    }


def rank_label(value, values, labels):
    order = sorted(values)
    pos = order.index(value) / max(1, len(order) - 1)
    return labels[min(len(labels) - 1, int(pos * len(labels)))]


def main():
    tracks = [analyze(p) for p in sorted(glob.glob(os.path.join(SRC, "*.mp3")))]
    energies = [t["loudness"] + 10 * t["density"] for t in tracks]

    lines = [
        "# Análise das músicas",
        "",
        "Gerado automaticamente por `tools/analyze_music.py`. São medições, não uma audição:",
        "andamento, tom estimado, energia e brilho ajudam a escolher o lugar de cada faixa.",
        "",
        "| Faixa | Duração | BPM | Tom | Energia | Brilho | Notas por segundo | Percussão | Dinâmica | Intensidade ao longo da faixa |",
        "|---|---|---|---|---|---|---|---|---|---|",
    ]
    for t, e in zip(tracks, energies):
        lines.append(
            f"| {t['name']} | {int(t['duration'] // 60)}:{int(t['duration'] % 60):02d} | {t['tempo']:.0f} | {t['key']} "
            f"| {rank_label(e, energies, ['baixa', 'média', 'alta'])} "
            f"| {rank_label(t['brightness'], [x['brightness'] for x in tracks], ['escuro', 'médio', 'brilhante'])} ({t['brightness']:.0f} Hz) "
            f"| {t['density']:.1f} | {t['percussive']:.2f} | {t['dynamics']:.0f} dB | `{sparkline(t['curve'])}` |"
        )
    lines += [
        "",
        "- **Energia:** volume médio + densidade de notas, comparado entre as faixas.",
        "- **Brilho:** centro do espectro; mais alto = som mais agudo e aberto, mais baixo = mais grave e fechado.",
        "- **Percussão:** quanto da energia vem de batidas em relação à parte melódica.",
        "- **Dinâmica:** diferença entre os trechos mais altos e os mais baixos.",
        "",
    ]
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("\n".join(lines))


if __name__ == "__main__":
    main()
