"""Monta o banco de instrumentos do jogo (assets/soundbank.bin) a partir de amostras
gravadas de orquestra em domínio público (CC0):

- VSCO 2 Community Edition  https://github.com/sgossner/VSCO-2-CE
- Versilian Community Sample Library  https://github.com/sgossner/VCSL

Igual ao N64: cada instrumento guarda poucas notas gravadas e o jogo muda a altura
delas para tocar as outras. As amostras são convertidas para 32 kHz mono, cortadas
e normalizadas. A altura de cada gravação é medida no próprio áudio.

Formato (little endian): "KGSB", u32 versão, u32 nº de instrumentos; para cada um:
u8 sem_altura, u8 nº de amostras; para cada amostra: f32 nota MIDI base, u32 nº de
quadros, i16[quadros].
"""
import io
import json
import os
import re
import struct
import urllib.parse
import urllib.request

import librosa
import numpy as np
import soundfile as sf

RATE = 32000
OUT = "assets/soundbank.bin"
DOC = "docs/SOUNDBANK.md"
REPOS = {"VSCO": ("sgossner/VSCO-2-CE", "master"), "VCSL": ("sgossner/VCSL", "master")}

# (nome, repo, pasta, filtro no nome do arquivo, sem altura?, duração máxima em s, máximo de amostras)
# A ordem é a mesma do enum InstrumentId em src/game.h.
INSTRUMENTS = [
    ("Trompas",          "VSCO", "Brass/F Horn/sus",                         r"_v3_",      False, 2.6, 7),
    ("Trompetes",        "VSCO", "Brass/Trumpet/sus",                        r"_v3_",      False, 2.6, 7),
    ("Trombones",        "VSCO", "Brass/Tenor Trombone/sus",                 r"_v3_",      False, 2.6, 7),
    ("Tuba",             "VSCO", "Brass/Tuba/sus",                           r"",          False, 2.6, 6),
    ("Violinos",         "VSCO", "Strings/Violin Section/susVib",            r"_v2",       False, 2.6, 8),
    ("Violinos curtos",  "VSCO", "Strings/Violin Section/Spic",              r"_v2_rr1",   False, 0.8, 8),
    ("Violoncelos",      "VSCO", "Strings/Cello Section/susvib",             r"_v3_",      False, 2.6, 7),
    ("Violoncelos curtos", "VSCO", "Strings/Cello Section/spic",             r"",          False, 0.8, 7),
    ("Contrabaixo",      "VSCO", "Strings/Solo Contrabass/SusVib",           r"",          False, 2.6, 6),
    ("Harpa",            "VSCO", "Strings/Harp",                             r"",          False, 2.0, 8),
    ("Flauta",           "VSCO", "Woodwinds/Flute/susvib",                   r"",          False, 2.6, 7),
    ("Oboé",             "VSCO", "Woodwinds/Oboe/Vib",                       r"",          False, 2.6, 7),
    ("Ocarina",          "VCSL", "Aerophones/Edge-blown Aerophones/Ocarina, Typical/Sustains/SusVib", r"", False, 2.6, 7),
    ("Glockenspiel",     "VSCO", "Percussion/Glock",                         r"",          False, 1.5, 6),
    ("Tímpanos",         "VSCO", "Percussion/Timpani",                       r"Hit_v3_rr1", False, 2.0, 6),
    ("Caixa marcial",    "VSCO", "VSCO 1 Percussion/drums/snare/drum3_marching", r"",      True,  0.8, 1),
    ("Bumbo de orquestra", "VSCO", "VSCO 1 Percussion/drums/bass",           r"",          True,  1.4, 1),
    ("Pratos",           "VSCO", "VSCO 1 Percussion/varMetal/Cymbals/clash", r"",          True,  2.5, 1),
    ("Tambor de guerra", "VSCO", "VSCO 1 Percussion/drums/other/ethnic/giant/mallet", r"", True,  1.5, 1),
    ("Gongo",            "VSCO", "VSCO 1 Percussion/varMetal/Gong/gong",     r"",          True,  3.0, 1),
]


def api_tree(repo, branch):
    req = urllib.request.Request(f"https://api.github.com/repos/{repo}/git/trees/{branch}?recursive=1")
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req) as r:
        return [t["path"] for t in json.load(r)["tree"] if t["type"] == "blob"]


def download(repo, branch, path):
    url = f"https://raw.githubusercontent.com/{repo}/{branch}/{urllib.parse.quote(path)}"
    with urllib.request.urlopen(url) as r:
        data, sr = sf.read(io.BytesIO(r.read()), always_2d=True)
    return librosa.resample(data.mean(axis=1).astype(np.float32), orig_sr=sr, target_sr=RATE)


def measure_pitch(y):
    f0, voiced, _ = librosa.pyin(y[: RATE * 2], fmin=25, fmax=2500, sr=RATE, frame_length=4096)
    f0 = f0[voiced] if voiced is not None and np.any(voiced) else f0[~np.isnan(f0)]
    if len(f0) == 0:
        return None
    return float(librosa.hz_to_midi(np.median(f0)))


def prepare(y, max_seconds):
    # Corta o silêncio do início, limita a duração e suaviza o final
    start = np.argmax(np.abs(y) > 0.01 * np.max(np.abs(y))) if np.any(y) else 0
    y = y[max(0, start - 32): max(0, start - 32) + int(max_seconds * RATE)]
    fade = min(len(y), int(0.08 * RATE))
    y[-fade:] *= np.linspace(1.0, 0.0, fade)
    peak = np.max(np.abs(y)) or 1.0
    return np.clip(y / peak * 0.9, -1, 1)


def main():
    trees = {k: api_tree(*v) for k, v in REPOS.items()}
    bank = []
    doc = ["# Banco de instrumentos", "",
           "Amostras de orquestra gravada em domínio público (CC0), tratadas no estilo N64 (32 kHz, mono).", "",
           "- **VSCO 2 Community Edition**, Versilian Studios: https://github.com/sgossner/VSCO-2-CE",
           "- **Versilian Community Sample Library (VCSL)**: https://github.com/sgossner/VCSL", "",
           "| # | Instrumento | Notas gravadas usadas |", "|---|---|---|"]

    for idx, (name, src, folder, pattern, unpitched, max_s, max_n) in enumerate(INSTRUMENTS):
        repo, branch = REPOS[src]
        files = sorted(p for p in trees[src]
                       if p.lower().endswith(".wav") and os.path.dirname(p) == folder and re.search(pattern, os.path.basename(p)))
        print(f"{name}: {len(files)} arquivos", flush=True)

        samples = []
        for path in files[: 1 if unpitched else 40]:
            y = download(repo, branch, path)
            root = 60.0 if unpitched else measure_pitch(y)
            if root is None:
                continue
            samples.append((root, prepare(y, max_s)))

        # Fica com notas espalhadas pela extensão do instrumento, sem repetir regiões
        samples.sort(key=lambda s: s[0])
        chosen = []
        for root, y in samples:
            if not chosen or root - chosen[-1][0] >= 3.5:
                chosen.append((root, y))
        while len(chosen) > max_n:
            chosen = chosen[::2] if len(chosen) > 2 * max_n else chosen[:max_n]

        bank.append((unpitched, chosen))
        notes = ", ".join(librosa.midi_to_note(round(r)) for r, _ in chosen) if not unpitched else "(percussão)"
        doc.append(f"| {idx} | {name} | {notes} |")
        print(f"  -> {notes}", flush=True)

    with open(OUT, "wb") as f:
        f.write(b"KGSB")
        f.write(struct.pack("<II", 1, len(bank)))
        for unpitched, chosen in bank:
            f.write(struct.pack("<BB", 1 if unpitched else 0, len(chosen)))
            for root, y in chosen:
                pcm = (y * 32767).astype("<i2")
                f.write(struct.pack("<fI", root, len(pcm)))
                f.write(pcm.tobytes())

    size = os.path.getsize(OUT) / 1e6
    doc += ["", f"Tamanho do banco: {size:.1f} MB", ""]
    with open(DOC, "w", encoding="utf-8") as f:
        f.write("\n".join(doc))
    print(f"Banco salvo: {size:.1f} MB")


if __name__ == "__main__":
    main()
