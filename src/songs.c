#include "game.h"

// Músicas do jogo em MML (ver synth.c): c d e f g a b, + sustenido, número = duração
// (1 semibreve, 2 mínima, 4 semínima, 8 colcheia, 16 semicolcheia), "." pontua,
// o4 = oitava (o4 c = dó central), < > desce/sobe oitava, [ ]n repete n vezes,
// (o3b o4d f+)1 = acorde. Na percussão, "c" na oitava 4 toca a gravação no tom original.

// ---------------------------------------------------------------------------
// Campo de Pangea, inspirado em "After Future" (docs/transcricao/after_future.md).
// Si menor, 126 BPM, 52 compassos. O "motor" é um galope de cordas graves (cavalgada)
// com tambores de guerra por baixo.
//   1-8   Abertura: pedal grave em Si, coral nascendo, trompas chamando     Bm... G F#
//   9-16  Tema A nas trompas                                              Bm G D A (2x)
//   17-24 Tema B (resposta) nos trompetes, coral em acordes               Em Bm G F# (2x)
//   25-32 Respiro: ocarina e harpa                                        G D A Bm G D Em F#
//   33-36 Tensão: pedal em Fá#, tudo crescendo                            F#
//   37-44 Clímax um tom acima, em Dó# menor, orquestra inteira            C#m A E B (2x)
//   45-48 Descida Dó#-Si-Lá-Sol# (a descida da música original)           C#m B A G#
//   49-52 Volta para Si menor                                             G G F# F#
// Camada de combate: tambores de guerra extras quando um inimigo persegue o jogador.
// ---------------------------------------------------------------------------
const Song SONG_FIELD = {
    .name = "Campo de Pangea",
    .bpm = 126.0f,
    .channelCount = 18,
    .channels = {
        // Trompas: chamado na abertura, Tema A, acordes, clímax e descida
        { INST_HORN, 0.80f, 0,
          "[r1]4 v13 (o3f+ o4f+)4. (o3a o4a)8 (o3b o4b)2 r2 (o3a o4a)4 (o3b o4b)4 (o4d o5d)2 (o3b o4b)2 (o3a+ o4a+)1"
          "o4 v15 f+4. a8 b2 >d4. c+8< b4 g4 a2. f+4 e2 >c+2<"
          "f+4. a8 b4 >d4< >e4. d8 c+4 d4< >f+2 e4. d8< >c+1<"
          "v13 [(o3g o3b)1 (o3f+ o4d)1 (o3g o4d)1 (o3f+ o3a+)1]2"
          "v10 (o3b o4d)1 (o3a o4d)1 (o3a o4c+)1 (o3b o4d)1 (o3b o4d)1 (o3a o4d)1 (o3g o3b)1 (o3a+ o4c+)1"
          "v13 (o3a+ o4c+)1 (o3a+ o4c+)1 v15 (o4c+ o4f+)1 (o4c+ o4f+)1"
          "o4 g+4. b8 >c+2< >e4. d+8 c+4< a4 b2. g+4 f+2 >d+2<"
          "g+4. b8 >c+4 e4< >f+4. e8 d+4 e4< >g+2 f+4. e8< >d+1<"
          "o4 c+2 e4 d+4 <b2 >d+4 c+4 <a2 >c+4 <b4 g+1"
          "(o3b o4d)1 (o3b o4d)1 (o3a+ o4c+)1 (o3a+ o4c+)1", -0.25f },

        // Trompetes: Tema B, fanfarra da tensão, clímax e descida uma oitava acima
        { INST_TRUMPET, 0.70f, 0,
          "[r1]16"
          "o4 v15 g4. f+8 e4 g4 f+2 d4 f+4 g4. a8 b4 >d4< >c+2.< a+4"
          ">e4. d8< b4 g4 f+4. g8 f+4 d4 b4 >d4 e4 d4< >c+1<"
          "[r1]8"
          "o4 v12 [f+8 f+16 f+16]4 v13 [a+8 a+16 a+16]4 v14 [>c+8 c+16 c+16<]4 v15 >c+2. c+8 c+8<"
          "o4 g+4. b8 >c+2< >e4. d+8 c+4< a4 b2. g+4 f+2 >d+2<"
          "g+4. b8 >c+4 e4< >f+4. e8 d+4 e4< >g+2 f+4. e8< >d+1<"
          "o5 c+2 e4 d+4 <b2 >d+4 c+4 <a2 >c+4 <b4 g+1"
          "v13 (o4b o5d)1 (o4b o5d)1 (o4a+ o5c+)2 r2 r1", 0.25f },

        // Trombones: quintas graves pulsando (o "peso" do naipe de metais)
        { INST_TROMBONE, 0.55f, 0,
          "v13 [r1]4 [(o2b o3f+)1]2 (o2g o3d)1 (o2f+ o3c+)1"
          "[(o2b o3f+)2 (o2b o3f+)2 (o2g o3d)2 (o2g o3d)2 (o3d o3a)2 (o3d o3a)2 (o2a o3e)2 (o2a o3e)2]2"
          "[(o2e o2b)2 (o2e o2b)2 (o2b o3f+)2 (o2b o3f+)2 (o2g o3d)2 (o2g o3d)2 (o2f+ o3c+)2 (o2f+ o3c+)2]2"
          "v10 (o2g o3d)1 (o3d o3a)1 (o2a o3e)1 (o2b o3f+)1 (o2g o3d)1 (o3d o3a)1 (o2e o2b)1 (o2f+ o3c+)1"
          "v12 (o2f+ o3c+)1 (o2f+ o3c+)1 v14 (o2f+ o3c+)1 (o2f+ o3c+)2 (o2f+ o3c+)8 (o2f+ o3c+)8 (o2f+ o3c+)8 (o2f+ o3c+)8"
          "v15 [(o2c+ o2g+)2 (o2c+ o2g+)2 (o2a o3e)2 (o2a o3e)2 (o2e o2b)2 (o2e o2b)2 (o2b o3f+)2 (o2b o3f+)2]2"
          "o3 c+2 e4 d+4 <b2 >d+4 c+4 <a2 >c+4 <b4 g+1"
          "(o2g o3d)1 (o2g o3d)1 (o2f+ o3c+)1 (o2f+ o3c+)1", 0.35f },

        // Tuba: notas base no fundo, a partir do Tema A
        { INST_TUBA, 0.60f, 0,
          "[r1]8 v14"
          "[o1b1 o1g1 o2d1 o1a1]2 [o1e1 o1b1 o1g1 o1f+1]2"
          "v11 o1g1 o2d1 o1a1 o1b1 o1g1 o2d1 o1e1 o1f+1"
          "v13 [o1f+1]4 v15 [o2c+1 o1a1 o1e1 o1b1]2 o2c+1 o1b1 o1a1 o1g+1 o1g1 o1g1 o1f+1 o1f+1", 0.0f },

        // Contrabaixos: o chão grave da música inteira
        { INST_CONTRABASS, 0.70f, 0,
          "v13 [o1b1]6 o1g1 o1f+1"
          "[o1b1 o1g1 o2d1 o1a1]2 [o1e1 o1b1 o1g1 o1f+1]2"
          "o1g1 o2d1 o1a1 o1b1 o1g1 o2d1 o1e1 o1f+1"
          "[o1f+1]4 v15 [o2c+1 o1a1 o1e1 o1b1]2 o2c+1 o1b1 o1a1 o1g+1 o1g1 o1g1 o1f+1 o1f+1", -0.1f },

        // Violoncelos sustentados dobrando o baixo uma oitava acima
        { INST_CELLOS, 0.50f, 0,
          "o2 v11 [b1]6 g1 f+1"
          "v13 [o2b1 o2g1 o3d1 o2a1]2 [o2e1 o2b1 o2g1 o2f+1]2"
          "v10 o2g1 o3d1 o2a1 o2b1 o2g1 o3d1 o2e1 o2f+1"
          "v13 [o2f+1]4 v15 [o3c+1 o2a1 o2e1 o2b1]2 o3c+1 o2b1 o2a1 o2g+1 o2g1 o2g1 o2f+1 o2f+1", 0.2f },

        // Violoncelos em galope: o motor da cavalgada
        { INST_CELLOS_SHORT, 0.60f, 0,
          "[r1]2 o2 v10 [[b8 b16 b16 b8 b8]2]4 v12 [g8 g16 g16 g8 g8]2 [f+8 f+16 f+16 f+8 f+8]2"
          "v13 [[b8 b16 b16 b8 b8]2 [g8 g16 g16 g8 g8]2 >[d8 d16 d16 d8 d8]2< [a8 a16 a16 a8 a8]2]2"
          "[[e8 e16 e16 e8 e8]2 [b8 b16 b16 b8 b8]2 [g8 g16 g16 g8 g8]2 [f+8 f+16 f+16 f+8 f+8]2]2"
          "[r1]8"
          "v12 [[f+8 f+16 f+16 f+8 f+8]2]2 v14 [f+16]16 v15 [f+16]16"
          "[>[c+8 c+16 c+16 c+8 c+8]2< [a8 a16 a16 a8 a8]2 [e8 e16 e16 e8 e8]2 [b8 b16 b16 b8 b8]2]2"
          ">[c+8 c+16 c+16 c+8 c+8]2< [b8 b16 b16 b8 b8]2 [a8 a16 a16 a8 a8]2 [g+8 g+16 g+16 g+8 g+8]2"
          "[g8 g16 g16 g8 g8]4 [f+8 f+16 f+16 f+8 f+8]4", 0.3f },

        // Violinos: acordes cheios lá em cima
        { INST_VIOLINS, 0.40f, 0,
          "[r1]6 v10 (o4g o4b o5d)1 (o4f+ o4a+ o5c+)1"
          "v12 [(o4f+ o4b o5d)1 (o4g o4b o5d)1 (o4f+ o4a o5d)1 (o4e o4a o5c+)1]2"
          "v13 [(o4g o4b o5e)1 (o4f+ o4b o5d)1 (o4g o4b o5d)1 (o4f+ o4a+ o5c+)1]2"
          "v10 (o4g o4b o5d)1 (o4f+ o4a o5d)1 (o4e o4a o5c+)1 (o4f+ o4b o5d)1 (o4g o4b o5d)1 (o4f+ o4a o5d)1 (o4e o4g o4b)1 (o4f+ o4a+ o5c+)1"
          "v12 (o4f+ o4a+ o5c+)1 (o4a+ o5c+ o5f+)1 v14 (o5c+ o5f+ o5a+)1 (o5f+ o5a+ o6c+)1"
          "v15 [(o5g+ o5b o6c+)1 (o5e o5a o6c+)1 (o5e o5g+ o5b)1 (o5d+ o5f+ o5b)1]2"
          "(o5e o5g+ o6c+)1 (o5d+ o5f+ o5b)1 (o5c+ o5e o5a)1 (o5c o5d+ o5g+)1"
          "v12 (o4g o4b o5d)1 (o4g o4b o5d)1 (o4f+ o4a+ o5c+)1 (o4f+ o4a+ o5c+)1", -0.45f },

        // Violinos em galope, uma oitava acima dos violoncelos
        { INST_VIOLINS_SHORT, 0.30f, 0,
          "[r1]8"
          "o4 v11 [[b8 b16 b16 b8 b8]2 [g8 g16 g16 g8 g8]2 >[d8 d16 d16 d8 d8]2< [a8 a16 a16 a8 a8]2]2"
          "v12 [[e8 e16 e16 e8 e8]2 [b8 b16 b16 b8 b8]2 [g8 g16 g16 g8 g8]2 [f+8 f+16 f+16 f+8 f+8]2]2"
          "[r1]8"
          "l16 v10 [o4f+ o4c+]8 v12 [o4f+ o4a+]8 v13 [o4a+ o5c+]8 v15 [o5c+ o5f+]8"
          "o4 v14 [>[c+8 c+16 c+16 c+8 c+8]2< [a8 a16 a16 a8 a8]2 [e8 e16 e16 e8 e8]2 [b8 b16 b16 b8 b8]2]2"
          "o5 [c+8 c+16 c+16 c+8 c+8]2 <[b8 b16 b16 b8 b8]2 [a8 a16 a16 a8 a8]2 [g+8 g+16 g+16 g+8 g+8]2>"
          "[r1]4", -0.3f },

        // Coral em acordes de três vozes
        { INST_CHOIR, 0.30f, 0,
          "[r1]2 v9 (o3f+ o3b o4d)1 v10 (o3f+ o3b o4d)1 v11 (o3f+ o3b o4c+)1 (o3f+ o3b o4d)1 (o3g o3b o4d)1 (o3f+ o3a+ o4c+)1"
          "[r1]8"
          "v12 [(o3g o3b o4e)1 (o3f+ o3b o4d)1 (o3g o3b o4d)1 (o3f+ o3a+ o4c+)1]2"
          "[r1]4 v10 (o3g o3b o4d)1 (o3f+ o3a o4d)1 (o3g o3b o4e)1 (o3f+ o3a+ o4c+)1"
          "v12 (o3f+ o3a+ o4c+)1 (o3a+ o4c+ o4f+)1 v14 (o4c+ o4f+ o4a+)1 (o4c+ o4f+ o4a+)1"
          "v15 [(o4c+ o4e o4g+)1 (o4c+ o4e o4a)1 (o3b o4e o4g+)1 (o3b o4d+ o4f+)1]2"
          "(o4c+ o4e o4g+)1 (o3b o4d+ o4f+)1 (o3a o4c+ o4e)1 (o3g+ o4c o4d+)1"
          "v13 (o3g o3b o4d)1 (o3g o3b o4d)1 (o3f+ o3a+ o4c+)1 (o3f+ o3a+ o4c+)1", 0.0f },

        // Ocarina: o respiro lírico
        { INST_OCARINA, 0.75f, 0,
          "[r1]24"
          "o5 v13 d4. e8 d4 <b4> <a2> d2 e4. f+8 e4 c+4 d2. <b4>"
          "d4. e8 g4 f+4 f+2 a2 g4. f+8 e4 d4 c+1", 0.1f },

        // Harpa: arpejos no respiro e no clímax
        { INST_HARP, 0.50f, 0,
          "[r1]24 l8 v11"
          "o3g b o4d g b g d o3b  o3d f+ a o4d f+ d o3a f+"
          "o3a o4c+ e a o5c+ o4a e c+  o3b o4d f+ b o5d o4b f+ d"
          "o3g b o4d g b g d o3b  o3d f+ a o4d f+ d o3a f+"
          "o3e g b o4e g e o3b g  o3f+ a+ o4c+ f+ a+ f+ c+ o3a+"
          "[r1]4 v13"
          "[o3c+ e g+ o4c+ e c+ o3g+ e  o3a o4c+ e a o5c+ o4a e c+"
          " o3e g+ b o4e g+ e o3b g+  o3b o4d+ f+ b o5d+ o4b f+ d+]2", -0.35f },

        // Tímpanos
        { INST_TIMPANI, 0.80f, 0,
          "o2 l4 v10 b4 r4 r2 r1 v11 b4 r4 r2 r1 v12 b4 r4 b4 r4 b4 r4 b4 r4 v13 g4 r4 g4 r4 l16 v12 [f+]8 v15 [f+]8"
          "l4 v14 [b4 r4 b8 b8 r4 g4 r4 g8 g8 r4 >d4< r4 >d8 d8< r4 a4 r4 l16 a a a a a a a a l4]2"
          "[e4 r4 e8 e8 r4 b4 r4 b8 b8 r4 g4 r4 g8 g8 r4 f+4 r4 l16 f+ f+ f+ f+ f+ f+ f+ f+ l4]2"
          "v10 [r1]6 l16 [f+]16 l4 f+4 r4 f+4 r4"
          "l16 v11 [f+]16 v12 [f+]16 v13 [f+]16 v15 [f+]16"
          "l4 [>c+4< r4 >c+8 c+8< r4 a4 r4 a8 a8 r4 e4 r4 e8 e8 r4 b4 r4 l16 b b b b b b b b l4]2"
          ">c+4 c+4 c+4 c+4< b4 b4 b4 b4 a4 a4 a4 a4 g+4 g+4 g+4 g+4"
          "g4 r4 r2 g4 r4 r2 l16 v12 [f+]16 v15 [f+]16", 0.0f },

        // Tambores de guerra: a base rítmica épica
        { INST_WARDRUM, 0.75f, 0,
          "o4 v9 [c2 r2]2 v11 [c4 r4 c4 r4]2 v13 [c4 r8 c8 r8 c8 c4]3 v15 c8 c8 c8 c8 c16 c16 c16 c16 c4"
          "v14 [[c4 r8 c8 r8 c8 c4]3 c8 c8 c8 c8 c16 c16 c16 c16 c4]4"
          "v8 [c2 r2]6 v11 [c4 r4]2 v13 c8 c8 c8 c8 c16 c16 c16 c16 c4"
          "v12 [c4 c4 c4 c4]2 v14 [c8]8 v15 [c16]16"
          "[[c4 r8 c8 r8 c8 c4]3 c8 c8 c8 c8 c16 c16 c16 c16 c4]2"
          "[c4 c4 c4 c4]3 [c16]16"
          "c1 r1 c2 c2 [c16]16", -0.15f },

        // Bumbo de orquestra nos tempos fortes
        { INST_BASSDRUM, 0.60f, 0,
          "o4 v12 c1 r1 c1 r1 [c2 c2]4"
          "v14 [c2 c4 c4]16"
          "v9 [c1]8"
          "v12 [c4 c4 c4 c4]4"
          "v15 [c4 c4 c2]8"
          "[c2 c2]4"
          "c1 r1 c1 r1", 0.0f },

        // Caixa (escura e discreta): só contratempo e rufos
        { INST_SNARE, 0.30f, 0,
          "o4 [r1]16 v8 [r4 c4 r4 c4]8 [r1]8"
          "l16 v6 [c]16 v8 [c]16 v10 [c]16 v13 [c]16"
          "v11 [r4 c4 r4 c4]8 [r1]8", 0.15f },

        // Pratos no começo das partes
        { INST_CYMBAL, 0.40f, 0,
          "o4 v13 [r1]8 c1 [r1]7 c1 [r1]7 [r1]8 [r1]4 c1 [r1]3 c1 [r1]3 c1 [r1]3 c1 [r1]3", 0.2f },

        // Camada de combate: tambores de guerra extras
        { INST_WARDRUM, 0.50f, 1,
          "o4 v12 [[c8 c16 c16]4]52", 0.15f },
    },
};
