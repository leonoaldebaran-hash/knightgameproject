#include "game.h"

// Músicas do jogo em MML (ver synth.c): c d e f g a b, + sustenido, número = duração
// (1 semibreve, 2 mínima, 4 semínima, 8 colcheia), "." pontua, o4 = oitava, [ ]n repete.

// ---------------------------------------------------------------------------
// Campo de Pangea, inspirado em "After Future" (transcrição em docs/transcricao/after_future.md):
// Si menor, baixo descendo Si-Lá-Sol-Fá# com saltos de oitava, motivo Fá#-Lá-Si.
// Parte A: Bm A G F# (2x) | Parte B: D A Bm F#m G A F# F#
// Camada de combate: bateria e metais entram quando um inimigo persegue o jogador.
// ---------------------------------------------------------------------------
const Song SONG_FIELD = {
    .name = "Campo de Pangea",
    .bpm = 104.0f,
    .channelCount = 7,
    .channels = {
        // Melodia na flauta
        { INST_FLUTE, 0.70f, 0,
          "o5 l4 v13"
          "f+4. a8 b2 | a4 e4 c+4 e4 | d4. e8 d8 c+8 <b4> | <a+2.> r4 |"
          "f+4. a8 b4 >d4< | >c+4.< b8 a2 | g4 f+4 e4 d4 | c+1 |"
          "a4. f+8 d4 f+4 | e4. c+8 <a2> | b4 >d4< b4 f+4 | a2 f+2 |"
          "g4. a8 b4 >d4< | e4. d8 c+4 e4 | f+2. e8 d8 | c+1 |" },

        // Harpa dedilhando os acordes
        { INST_HARP, 0.50f, 0,
          "l8 v11"
          "[o3b o4d f+ b o5d o4b f+ d  o3a o4c+ e a o5c+ o4a e c+"
          " o3g b o4d g b g d o3b  o3f+ a+ o4c+ f+ a+ f+ c+ o3a+]2"
          "o3a o4d f+ a o5d o4a f+ d  o3a o4c+ e a o5c+ o4a e c+"
          "o3b o4d f+ b o5d o4b f+ d  o3f+ a o4c+ f+ a f+ c+ o3a"
          "o3g b o4d g b g d o3b  o3a o4c+ e a o5c+ o4a e c+"
          "[o3f+ a+ o4c+ f+ a+ f+ c+ o3a+]2" },

        // Baixo pulando de oitava, descendo como na música original
        { INST_BASS, 0.90f, 0,
          "l8 v13"
          "[o1[b>b<]4 o1[a>a<]4 o1[g>g<]4 o1[f+>f+<]4]2"
          "o2[d>d<]4 o1[a>a<]4 o1[b>b<]4 o1[f+>f+<]4"
          "o1[g>g<]4 o1[a>a<]4 o1[f+>f+<]4 o1[f+>f+<]4" },

        // Cordas segurando a terça e a quinta de cada acorde
        { INST_STRINGS, 0.35f, 0,
          "v10 [o4d1 o4c+1 o3b1 o3a+1]2 o4f+1 o4c+1 o4d1 o3a1 o3b1 o4c+1 o3a+1 o3a+1" },
        { INST_STRINGS, 0.35f, 0,
          "v10 [o4f+1 o4e1 o4d1 o4c+1]2 o4a1 o4e1 o4f+1 o4c+1 o4d1 o4e1 o4c+1 o4c+1" },

        // Camada de combate: bateria (c = bumbo, d = caixa, f = chimbal)
        { INST_DRUMS, 0.60f, 1,
          "l8 v14 [c f d f c c d f]16" },

        // Camada de combate: metais marcando a nota base de cada acorde
        { INST_BRASS, 0.45f, 1,
          "o3 v13"
          "[b4 r8 b8 b4 r4 a4 r8 a8 a4 r4 g4 r8 g8 g4 r4 f+4 r8 f+8 f+4 r4]2"
          ">d4 r8 d8 d4 r4< a4 r8 a8 a4 r4 b4 r8 b8 b4 r4 f+4 r8 f+8 f+4 r4"
          "g4 r8 g8 g4 r4 a4 r8 a8 a4 r4 f+4 r8 f+8 f+4 r4 f+4 r8 f+8 f+2" },
    },
};
