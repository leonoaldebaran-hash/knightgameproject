#include "game.h"

// Músicas do jogo em MML (ver synth.c): c d e f g a b, + sustenido, número = duração
// (1 semibreve, 2 mínima, 4 semínima, 8 colcheia, 16 semicolcheia), "." pontua,
// o4 = oitava (o4 c = dó central), < > desce/sobe oitava, [ ]n repete n vezes.
// Na percussão sem altura, "c" na oitava 4 toca a gravação no tom original.

// ---------------------------------------------------------------------------
// Campo de Pangea, inspirado em "After Future" (docs/transcricao/after_future.md).
// Si menor, 132 BPM, 40 compassos:
//   1-4   Abertura: rufo de tímpano e fanfarra de trompetes        Bm Bm Bm F#
//   5-12  Tema nas trompas, cordas em semicolcheias                Bm G D A (2x)
//   13-20 Tema nos trompetes, trompas em contracanto, coral entra  Bm G D A (2x)
//   21-28 Ponte: ocarina e harpa, crescendo no fim                 G A F#m Bm G A F# F#
//   29-36 Clímax em Ré maior com a orquestra inteira               D A Bm G D A G A
//   37-40 Fecho: Si-Lá-Sol-Fá# da música original, metais juntos   Bm A G F#
// Camada de combate: tambores de guerra quando um inimigo persegue o jogador.
// ---------------------------------------------------------------------------
const Song SONG_FIELD = {
    .name = "Campo de Pangea",
    .bpm = 132.0f,
    .channelCount = 15,
    .channels = {
        { INST_HORN, 0.75f, 0,
          "o3 v13 b1 b1 b1 a+1"
          "o4 v14 f+4 b4 >c+4 d4< >d4. c+8< b2 a4 f+4 a4 >d4< >c+2.< r4"
          "f+4 b4 >c+4 d4< >e4. d8 c+4 d4< >f+4. e8 d4< a4 >e1<"
          "v12 o4d2 o4c+2 o3b1 o3a2 o4d2 o4c+1 o4d2 o4f+2 o4e1 o4d2 o4f+2 o4e1"
          "v10 o4d1 o4c+1 o4c+1 o4d1 o4d1 o4c+1 o3a+1 o4c+1"
          "v15 o4a2 o4f+2 o4e1 o4f+2 o4d2 o4d1 o4a2 o4f+2 o4a2 o4e2 o4g2 o4b2 o5c+1"
          "o3 b2 >d4 c+4< a2 >c+4< b4 g2 b4 a4 f+1", -0.25f },

        { INST_TRUMPET, 0.70f, 0,
          "o4 v14 r2 f+8 f+8 b4 >d2< b4 f+4 a4. b8 >c+4 d4< >c+1<"
          "[r1]8"
          "o4 v15 f+4 b4 >c+4 d4< >d4. c+8< b2 a4 f+4 a4 >d4< >c+2.< r4"
          "f+4 b4 >c+4 d4< >e4. d8 c+4 d4< >f+4. e8 d4< a4 >e1<"
          "[r1]6 o4 v12 f+8 f+8 f+8 f+8 a+8 a+8 a+8 a+8 v15 >c+2. c+8 c+8<"
          "o4 f+2 a4 >d4< >c+2.< a4 b4. a8 f+4 d4 e2. d4"
          "f+4. g8 a4 >d4< >e2 c+4< a4 b4 >d4 g4 f+4< >e1<"
          "o4 b2 >d4 c+4< a2 >c+4< b4 g2 b4 a4 f+1", 0.25f },

        { INST_TROMBONE, 0.50f, 0,
          "o2 v12 b1 b1 b1 f+1"
          "[o2b1 o2g1 o3d1 o2a1]4"
          "v11 o2g1 o2a1 o2f+1 o2b1 o2g1 o2a1 o2f+1 o2f+1"
          "v14 o3d1 o2a1 o2b1 o2g1 o3d1 o2a1 o2g1 o2a1"
          "v15 o2b2 >d4 c+4< a2 >c+4< b4 g2 b4 a4 f+1", 0.35f },

        { INST_TUBA, 0.55f, 0,
          "o1 v13 b1 b1 b1 f+1"
          "[o1b2 o1b2 o1g2 o1g2 o2d2 o2d2 o1a2 o1a2]4"
          "v11 o1g1 o1a1 o1f+1 o1b1 o1g1 o1a1 o1f+1 o1f+1"
          "v14 o2d1 o1a1 o1b1 o1g1 o2d1 o1a1 o1g1 o1a1"
          "v15 o1b1 o1a1 o1g1 o1f+1", 0.0f },

        { INST_VIOLINS, 0.45f, 0,
          "[r1]12"
          "v12 [o5f+1 o5g1 o5f+1 o5e1]2"
          "v10 o5d1 o5e1 o5c+1 o5d1 o5d1 o5e1 o5c+1 o5c+1"
          "v14 o5a2 o6d2 o6c+2 o5a2 o5b2 o6d2 o5b2 o5g2 o5a2 o6d2 o6e2 o6c+2 o6d2 o5b2 o6c+2 o6e2"
          "o5f+1 o5e1 o5d1 o5c+1", -0.45f },

        { INST_VIOLINS_SHORT, 0.35f, 0,
          "[r1]4"
          "l16 v11 [[o4b o5f+]8 [o4g o5d]8 [o4f+ o4a]8 [o4e o4a]8]4"
          "[r1]6 v10 [o4f+ o5c+]8 v13 [o4f+ o4a+]8"
          "v13 [[o4f+ o5d]8 [o4e o4a]8 [o4f+ o4b]8 [o4d o4g]8]2"
          "v14 [o4f+ o4b]8 [o4e o4a]8 [o4d o4g]8 [o4c+ o4f+]8", -0.3f },

        { INST_CELLOS_SHORT, 0.45f, 0,
          "l8 v12 [[o2b o3b]4]3 [o2f+ o3f+]4"
          "[[o2b o3b]4 [o2g o3g]4 [o3d o4d]4 [o2a o3a]4]4"
          "v11 [o2g o3g]4 [o2a o3a]4 [o2f+ o3f+]4 [o2b o3b]4 [o2g o3g]4 [o2a o3a]4 [o2f+ o3f+]4 [o2f+ o3f+]4"
          "v13 [[o3d o4d]4 [o2a o3a]4 [o2b o3b]4 [o2g o3g]4]2"
          "v14 [o2b o3b]4 [o2a o3a]4 [o2g o3g]4 [o2f+ o3f+]4", 0.3f },

        { INST_CHOIR, 0.35f, 0,
          "[r1]12"
          "v11 [o4f+1 o4g1 o4f+1 o4e1]2"
          "[r1]4 v10 o4d1 o4e1 o4c+1 o4c+1"
          "v14 o4f+1 o4e1 o4f+1 o4d1 o4f+1 o4e1 o4d1 o4e1"
          "o4f+1 o4e1 o4d1 o4c+1", 0.0f },

        { INST_OCARINA, 0.70f, 0,
          "[r1]20"
          "o5 v13 d4. e8 d4 <b4> c+2 e2 f+4. e8 c+4 <a4> <b1>"
          "d4. e8 g4 f+4 e2. a4 a+4. g+8 f+4 e4 f+1", 0.1f },

        { INST_HARP, 0.45f, 0,
          "[r1]20 l8 v11"
          "o3g b o4d g b g d o3b  o3a o4c+ e a o5c+ o4a e c+"
          "o3f+ a o4c+ f+ a f+ c+ o3a  o3b o4d f+ b o5d o4b f+ d"
          "o3g b o4d g b g d o3b  o3a o4c+ e a o5c+ o4a e c+"
          "[o3f+ a+ o4c+ f+ a+ f+ c+ o3a+]2"
          "v13 o4d f+ a o5d f+ d o4a f+  o3a o4c+ e a o5c+ o4a e c+"
          "o3b o4d f+ b o5d o4b f+ d  o3g b o4d g b g d o3b"
          "o4d f+ a o5d f+ d o4a f+  o3a o4c+ e a o5c+ o4a e c+"
          "o3g b o4d g b g d o3b  o3a o4c+ e a o5c+ o4a e c+", -0.35f },

        { INST_TIMPANI, 0.75f, 0,
          "o2 l16 v8 [b]16 v10 [b]16 v12 [b]16 l4 v15 f+4 f+4 f+8 f+8 f+8 f+8"
          "v14 [b4 r4 b4 r4 g4 r4 g4 r4 >d4< r4 >d4< r4 a4 r4 a8 a8 a8 a8]4"
          "[r1]6 l16 v9 [f+]16 v14 [f+]16"
          "l4 v15 [>d4< r4 >d4< r4 a4 r4 a4 r4 b4 r4 b4 r4 g4 r4 g8 g8 g8 g8]2"
          "b4 b4 b4 b4 a4 a4 a4 a4 g4 g4 g4 g4 f+8 f+8 f+8 f+8 f+8 f+8 f+8 f+8", 0.0f },

        { INST_SNARE, 0.40f, 0,
          "o4 [r1]2 l32 v7 [c]32 v13 [c]32"
          "v11 [c8 c16 c16 c8 c8 c8 c16 c16 c8 c8]16"
          "[r1]6 l16 v8 [c]16 v14 [c]16"
          "v13 [c8 c16 c16 c8 c8 c8 c16 c16 c8 c8]8"
          "v14 [c8 c16 c16 c8 c8 c8 c16 c16 c8 c8]3 l16 [c]16", 0.15f },

        { INST_BASSDRUM, 0.60f, 0,
          "o4 v15 c1 c1 c1 c4 c4 c4 c4"
          "v13 [c4 r4 c4 r4]16"
          "v11 [c2 r2]8"
          "v15 [c4 r4 c4 c8 c8]8"
          "[c4 c4 c4 c4]4", 0.0f },

        { INST_CYMBAL, 0.45f, 0,
          "o4 v14 [r1]4 c1 [r1]7 c1 [r1]7 [r1]8 c1 [r1]3 c1 [r1]3 c1 [r1]3", 0.2f },

        // Camada de combate: tambores de guerra
        { INST_WARDRUM, 0.60f, 1,
          "o4 l8 v14 [c c r c c r c r]40", -0.1f },
    },
};
