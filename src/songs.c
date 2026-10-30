#include "game.h"

// Músicas do jogo em MML (ver synth.c): c d e f g a b, + sustenido, número = duração
// (1 semibreve, 2 mínima, 4 semínima, 8 colcheia, 16 semicolcheia), "." pontua,
// o4 = oitava, < > desce/sobe oitava, [ ]n repete n vezes.
// Bateria: c bumbo, d caixa, f chimbal, g tom, a prato.

// ---------------------------------------------------------------------------
// Campo de Pangea, inspirado em "After Future" (docs/transcricao/after_future.md):
// Si menor, baixo pulando de oitava, motivo Fá#-Lá-Si. Arranjo épico de cavalgada:
// metais na melodia, coral, cordas em semicolcheias, tímpano e bateria marcial.
// Parte A: Bm G D A (2x) | Parte B, o clímax: Bm A G F# (2x)
// Camada de combate: bateria em semicolcheias quando um inimigo persegue o jogador.
// ---------------------------------------------------------------------------
const Song SONG_FIELD = {
    .name = "Campo de Pangea",
    .bpm = 132.0f,
    .channelCount = 8,
    .channels = {
        // Metais: a melodia heroica
        { INST_BRASS, 0.75f, 0,
          "o4 l4 v14"
          "f+4 b4 >c+4 d4< | >d4. c+8< b2 | a4 f+4 a4 >d4< | >c+2.< r4 |"
          "f+4 b4 >c+4 d4< | >e4. d8 c+4 d4< | >f+4. e8 d4< a4 | >e1< |"
          ">f+2 d4 f+4< | >e2 c+4 e4< | >d4. e8 d4< b4 | a+2. >c+4< |"
          ">f+4. g8 f+4 e4< | >e4. f+8 e4 c+4< | >d4< b4 >d4 g4< | >f+1< |" },

        // Cordas dobrando a melodia uma oitava acima, para dar brilho
        { INST_STRINGS, 0.30f, 0,
          "o5 l4 v12"
          "f+4 b4 >c+4 d4< | >d4. c+8< b2 | a4 f+4 a4 >d4< | >c+2.< r4 |"
          "f+4 b4 >c+4 d4< | >e4. d8 c+4 d4< | >f+4. e8 d4< a4 | >e1< |"
          ">f+2 d4 f+4< | >e2 c+4 e4< | >d4. e8 d4< b4 | a+2. >c+4< |"
          ">f+4. g8 f+4 e4< | >e4. f+8 e4 c+4< | >d4< b4 >d4 g4< | >f+1< |" },

        // Cordas em semicolcheias: o motor que empurra a música para a frente
        { INST_STACCATO, 0.35f, 0,
          "l16 v12"
          "[[o3b o4f+]8 [o3g o4d]8 [o3d a]8 [o3a o4e]8]2"
          "[[o3b o4f+]8 [o3a o4e]8 [o3g o4d]8 [o3f+ o4c+]8]2" },

        // Baixo pulando de oitava, como na música original
        { INST_BASS, 0.80f, 0,
          "l8 v14"
          "[o1[b>b<]4 o1[g>g<]4 o2[d>d<]4 o1[a>a<]4]2"
          "[o1[b>b<]4 o1[a>a<]4 o1[g>g<]4 o1[f+>f+<]4]2" },

        // Coral segurando os acordes
        { INST_CHOIR, 0.35f, 0,
          "v12 [o4f+1 o4g1 o4f+1 o4e1]2 [o4f+1 o4e1 o4d1 o4c+1]2" },

        // Bateria marcial: prato no começo de cada frase e virada de tom no fim
        { INST_DRUMS, 0.55f, 0,
          "v14 [a8 r8 d8 r8 c8 c8 d8 r8 [c8 r8 d8 r8 c8 c8 d8 r8]2"
          " g16g16g16g16 g8g8 d16d16d16d16 d8d8]4" },

        // Tímpano marcando a nota base, com rufo no fim de cada frase
        { INST_TIMPANI, 0.70f, 0,
          "o2 l4 v14"
          "[b4 r4 b4 r4 g4 r4 g4 r4 d4 r4 d4 r4 a8a8a8a8a8a8a8a8]2"
          "[b4 r4 b4 r4 a4 r4 a4 r4 g4 r4 g4 r4 f+8f+8f+8f+8f+8f+8f+8f+8]2" },

        // Camada de combate: bateria em semicolcheias
        { INST_DRUMS, 0.50f, 1,
          "l16 v13 [c f f f d f f f c f c f d f g g]16" },
    },
};
