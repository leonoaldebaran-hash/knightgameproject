#include "game.h"

// Músicas do jogo em MML (ver synth.c): c d e f g a b, + sustenido, número = duração
// (1 semibreve, 2 mínima, 4 semínima, 8 colcheia, 16 semicolcheia), "." pontua,
// o4 = oitava (o4 c = dó central), < > desce/sobe oitava, [ ]n repete n vezes,
// (o3b o4d f+)1 = acorde, k12 / k-12 transpõe as notas seguintes uma oitava (k0 volta).
// Na percussão, "c" na oitava 4 toca a gravação no tom original.

// ---------------------------------------------------------------------------
// Campo de Pangea: melodia cantada pelo autor (assets/hum/tema_campo.m4a,
// transcrição em docs/transcricao/tema_campo.md), arranjada para orquestra.
// Si menor, 150 BPM (cavalgada), 40 compassos:
//   1-4   Abertura: galope começa, coral e trompas                 Bm Bm G F#
//   5-12  Tema nas trompas (frase principal + resposta)           Bm G C Bm F# F# Bm G
//   13-20 Tema nos trompetes e violinos, coral entra              (mesmos acordes)
//   21-24 Trecho cantado grave, na ocarina                        C E B Em
//   25-28 Subida cromática preparando o clímax                    B G B B
//   29-36 Clímax com a orquestra inteira                          G Em D Bm F# F# G Bm
//   37-40 Fecho cantado: Sol Sol Mi, Ré# Mi, Dó Si                C B C F#
// Camada de combate: tambores de guerra extras quando um inimigo persegue o jogador.
// ---------------------------------------------------------------------------

// Frase principal: sobe Mi Fá# Sol Lá, salta para Ré, desce Dó# Dó Si
#define THEME_A "r8 o4e8 o4f+8 o4g8 o4a4 o5d4 o5d4. o5c+8 o4b2 r8 o4e8 o4f+8 o4g8 o4g8 o4g8 o4a4 o5c+8 o5c8 o4b2. "
// Resposta: Lá# Sol# Lá# Fá#, depois sobe até Mi e volta ao Si
#define THEME_B "o4a+8 o4g+8 o4a+4 o4f+2 o5d8 o4a+8 o4a+8 o5c+8 o5d8 o5e8 o5c+8 o4b8 o4b2. r4 r1 "
// Trecho grave e marcado
#define THEME_LOW "o5c8 o5d8 o5e4 o5e8 o5e8 o5e4 o5g+4 o5f+2 o5c+4 o5f+8 o5e8 o5d+4 o5d+2 o4b8 o4b8 o5c8 o5d8 o5e4 o5d8 o5c+8 "
// Subida cromática até o clímax
#define BUILD "o3b8 o3b8 o4c8 o4c8 o4d+4 o4e8 o4e8 o4f8 o4f8 o4g4 o4f+4 o4d8 o4c+8 o4e4 o4d+2. o4d+2 o4e8 o4f+8 o4g8 o4a8 "
// Clímax: o Ré longo com o cromatismo, a frase principal e a resposta
#define CLIMAX "o5d2. o5c+8 o5c8 o4b2 r8 o4e8 o4f+8 o4g8 o4a4 o5d4 o5d4. o5c+8 o4b1 o4a+8 o4g+8 o4a+4 o4f+2 o5d8 o4a+8 o4a+8 o5c+8 o5d8 o5e8 o5c+8 o4b8 o4b2. o4a+4 o4b1 "
// Fecho
#define OUTRO "o4g2 o4g4 o5e4 o5d+4 o5e2. o5c4 o4b2. r1 "

#define DRUM_BAR  "c4 r8 c8 r8 c8 c4 "
#define DRUM_FILL "c8 c8 c8 c8 c16 c16 c16 c16 c4 "

const Song SONG_FIELD = {
    .name = "Campo de Pangea",
    .bpm = 150.0f,
    .channelCount = 21,
    .channels = {
        // Trompas: tema, acordes, subida e clímax uma oitava abaixo dos trompetes
        { INST_HORN, 0.80f, 0,
          "v12 (o3f+ o3b)1 (o3f+ o3b)1 (o3g o3b)1 (o3f+ o3a+)1"
          "v15 " THEME_A THEME_B
          "v12 (o3f+ o3b)1 (o3g o3b)1 (o3g o4c)1 (o3f+ o3b)1 (o3f+ o3a+)1 (o3f+ o3a+)1 (o3f+ o3b)1 (o3g o3b)1"
          "v10 (o3g o4c)1 (o3g+ o3b)1 (o3f+ o3b)1 (o3g o3b)1"
          "v14 " BUILD
          "v15 k-12 " CLIMAX OUTRO "k0", -0.25f },

        // Trompetes: o tema na segunda vez, fim da subida, clímax e fecho
        { INST_TRUMPET, 0.70f, 0,
          "[r1]12 v15 " THEME_A THEME_B
          "[r1]6 v14 o4e4 o4d+2. o4d+2 o4e8 o4f+8 o4g8 o4a8"
          "v15 " CLIMAX "v14 " OUTRO, 0.25f },

        // Violinos dobrando a melodia uma oitava acima
        { INST_VIOLINS, 0.40f, 0,
          "[r1]12 v12 k12 " THEME_A THEME_B "k0 [r1]4 v13 k12 " BUILD "v15 " CLIMAX "v13 " OUTRO "k0", -0.45f },

        // Violinos em acordes sustentados
        { INST_VIOLINS, 0.32f, 0,
          "[r1]4 v11 (o4f+ o4b o5d)1 (o4g o4b o5d)1 (o4g o5c o5e)1 (o4f+ o4b o5d)1 (o4f+ o4a+ o5c+)1 (o4f+ o4a+ o5c+)1 (o4f+ o4b o5d)1 (o4g o4b o5d)1"
          "[r1]8 v10 (o4g o5c o5e)1 (o4g+ o4b o5e)1 (o4f+ o4b o5d+)1 (o4g o4b o5e)1", -0.35f },

        // Violinos em galope
        { INST_VIOLINS_SHORT, 0.30f, 0,
          "[r1]12 v11 [o4b8 o4b16 o4b16 o4b8 o4b8]2 [o4g8 o4g16 o4g16 o4g8 o4g8]2 [o5c8 o5c16 o5c16 o5c8 o5c8]2 [o4b8 o4b16 o4b16 o4b8 o4b8]2"
          "[o4f+8 o4f+16 o4f+16 o4f+8 o4f+8]2 [o4f+8 o4f+16 o4f+16 o4f+8 o4f+8]2 [o4b8 o4b16 o4b16 o4b8 o4b8]2 [o4g8 o4g16 o4g16 o4g8 o4g8]2"
          "[r1]8 v14 [o4g8 o4g16 o4g16 o4g8 o4g8]2 [o4e8 o4e16 o4e16 o4e8 o4e8]2 [o5d8 o5d16 o5d16 o5d8 o5d8]2 [o4b8 o4b16 o4b16 o4b8 o4b8]2"
          "[o4f+8 o4f+16 o4f+16 o4f+8 o4f+8]2 [o4f+8 o4f+16 o4f+16 o4f+8 o4f+8]2 [o4g8 o4g16 o4g16 o4g8 o4g8]2 [o4b8 o4b16 o4b16 o4b8 o4b8]2", -0.3f },

        // Violoncelos em galope: o motor da cavalgada
        { INST_CELLOS_SHORT, 0.60f, 0,
          "[r1]2 v11 [o2g8 o2g16 o2g16 o2g8 o2g8]2 [o2f+8 o2f+16 o2f+16 o2f+8 o2f+8]2"
          "v13 [[o2b8 o2b16 o2b16 o2b8 o2b8]2 [o2g8 o2g16 o2g16 o2g8 o2g8]2 [o3c8 o3c16 o3c16 o3c8 o3c8]2 [o2b8 o2b16 o2b16 o2b8 o2b8]2"
          " [o2f+8 o2f+16 o2f+16 o2f+8 o2f+8]2 [o2f+8 o2f+16 o2f+16 o2f+8 o2f+8]2 [o2b8 o2b16 o2b16 o2b8 o2b8]2 [o2g8 o2g16 o2g16 o2g8 o2g8]2]2"
          "[r1]4 v12 [o2b8 o2b16 o2b16 o2b8 o2b8]2 [o2g8 o2g16 o2g16 o2g8 o2g8]2 [o2b8 o2b16 o2b16 o2b8 o2b8]2 [o2b8 o2b16 o2b16 o2b8 o2b8]2"
          "v15 [o2g8 o2g16 o2g16 o2g8 o2g8]2 [o2e8 o2e16 o2e16 o2e8 o2e8]2 [o3d8 o3d16 o3d16 o3d8 o3d8]2 [o2b8 o2b16 o2b16 o2b8 o2b8]2"
          "[o2f+8 o2f+16 o2f+16 o2f+8 o2f+8]2 [o2f+8 o2f+16 o2f+16 o2f+8 o2f+8]2 [o2g8 o2g16 o2g16 o2g8 o2g8]2 [o2b8 o2b16 o2b16 o2b8 o2b8]2"
          "[o3c8 o3c16 o3c16 o3c8 o3c8]2 [o2b8 o2b16 o2b16 o2b8 o2b8]2 [o3c8 o3c16 o3c16 o3c8 o3c8]2 [o2f+8 o2f+16 o2f+16 o2f+8 o2f+8]2", 0.3f },

        // Violoncelos sustentados; na subida tocam a linha cromática junto com as trompas
        { INST_CELLOS, 0.50f, 0,
          "v12 o2b1 o2b1 o2g1 o2f+1 [o2b1 o2g1 o3c1 o2b1 o2f+1 o2f+1 o2b1 o2g1]2 o3c1 o2e1 o2b1 o2e1"
          "v14 k-12 " BUILD "k0 v15 o2g1 o2e1 o3d1 o2b1 o2f+1 o2f+1 o2g1 o2b1 o3c1 o2b1 o3c1 o2f+1", 0.2f },

        // Contrabaixos: o chão grave
        { INST_CONTRABASS, 0.70f, 0,
          "v13 o1b1 o1b1 o1g1 o1f+1 [o1b1 o1g1 o2c1 o1b1 o1f+1 o1f+1 o1b1 o1g1]2"
          "o2c1 o1e1 o1b1 o1e1 o1b1 o1g1 o1b1 o1b1 v15 o1g1 o1e1 o2d1 o1b1 o1f+1 o1f+1 o1g1 o1b1 o2c1 o1b1 o2c1 o1f+1", -0.1f },

        // Tuba
        { INST_TUBA, 0.60f, 0,
          "[r1]4 v14 [o1b1 o1g1 o2c1 o1b1 o1f+1 o1f+1 o1b1 o1g1]2"
          "v11 o2c1 o1e1 o1b1 o1e1 o1b1 o1g1 o1b1 o1b1 v15 o1g1 o1e1 o2d1 o1b1 o1f+1 o1f+1 o1g1 o1b1 o2c1 o1b1 o2c1 o1f+1", 0.0f },

        // Trombones: quintas graves
        { INST_TROMBONE, 0.55f, 0,
          "v11 (o2b o3f+)1 (o2b o3f+)1 (o2g o3d)1 (o2f+ o3c+)1"
          "v13 [(o2b o3f+)2 (o2b o3f+)2 (o2g o3d)2 (o2g o3d)2 (o3c o3g)2 (o3c o3g)2 (o2b o3f+)2 (o2b o3f+)2"
          " (o2f+ o3c+)2 (o2f+ o3c+)2 (o2f+ o3c+)2 (o2f+ o3c+)2 (o2b o3f+)2 (o2b o3f+)2 (o2g o3d)2 (o2g o3d)2]2"
          "v11 (o3c o3g)1 (o2e o2b)1 (o2b o3f+)1 (o2e o2b)1 (o2b o3f+)1 (o2g o3d)1 (o2b o3f+)1 (o2b o3f+)1"
          "v15 (o2g o3d)2 (o2g o3d)4 (o2g o3d)4 (o2e o2b)2 (o2e o2b)4 (o2e o2b)4 (o3d o3a)2 (o3d o3a)4 (o3d o3a)4 (o2b o3f+)2 (o2b o3f+)4 (o2b o3f+)4"
          "(o2f+ o3c+)2 (o2f+ o3c+)4 (o2f+ o3c+)4 (o2f+ o3c+)2 (o2f+ o3c+)4 (o2f+ o3c+)4 (o2g o3d)2 (o2g o3d)4 (o2g o3d)4 (o2b o3f+)2 (o2b o3f+)4 (o2b o3f+)4"
          "(o3c o3g)1 (o2b o3f+)1 (o3c o3g)1 (o2f+ o3c+)1", 0.35f },

        // Coral em acordes
        { INST_CHOIR, 0.30f, 0,
          "v9 (o3f+ o3b o4d)1 (o3f+ o3b o4d)1 (o3g o3b o4d)1 (o3f+ o3a+ o4c+)1 [r1]8"
          "v12 (o3f+ o3b o4d)1 (o3g o3b o4d)1 (o3g o4c o4e)1 (o3f+ o3b o4d)1 (o3f+ o3a+ o4c+)1 (o3f+ o3a+ o4c+)1 (o3f+ o3b o4d)1 (o3g o3b o4d)1"
          "v11 (o3g o4c o4e)1 (o3g+ o3b o4e)1 (o3f+ o3b o4d+)1 (o3g o3b o4e)1 (o3f+ o3b o4d+)1 (o3g o3b o4d)1 (o3f+ o3b o4d+)1 (o3f+ o3b o4d+)1"
          "v15 (o3g o3b o4d)1 (o3g o3b o4e)1 (o3f+ o3a o4d)1 (o3f+ o3b o4d)1 (o3f+ o3a+ o4c+)1 (o3f+ o3a+ o4c+)1 (o3g o3b o4d)1 (o3f+ o3b o4d)1"
          "v13 (o3g o4c o4e)1 (o3f+ o3b o4d+)1 (o3g o4c o4e)1 (o3f+ o3a+ o4c+)1", 0.0f },

        // Ocarina: o trecho grave cantado
        { INST_OCARINA, 0.75f, 0, "[r1]20 v13 " THEME_LOW, 0.1f },

        // Harpa
        { INST_HARP, 0.45f, 0,
          "[r1]4 l8 v9 o3b o4d f+ b o5d o4b f+ d o3g b o4d g b g d o3b o3c e g o4c e c o3g e o3b o4d f+ b o5d o4b f+ d"
          "o3f+ a+ o4c+ f+ a+ f+ c+ o3a+ o3f+ a+ o4c+ f+ a+ f+ c+ o3a+ o3b o4d f+ b o5d o4b f+ d o3g b o4d g b g d o3b"
          "[r1]8 v11 o3c e g o4c e c o3g e  o3e g+ b o4e g+ e o3b g+  o3b o4d+ f+ b o5d+ o4b f+ d+  o3e g b o4e g e o3b g"
          "[r1]4 v13 o3g b o4d g b g d o3b o3e g b o4e g e o3b g o3d f+ a o4d f+ d o3a f+ o3b o4d f+ b o5d o4b f+ d"
          "o3f+ a+ o4c+ f+ a+ f+ c+ o3a+ o3f+ a+ o4c+ f+ a+ f+ c+ o3a+ o3g b o4d g b g d o3b o3b o4d f+ b o5d o4b f+ d", -0.35f },

        // Tímpanos
        { INST_TIMPANI, 0.80f, 0,
          "l4 v11 o2b2 r2 o2b2 r2 o2g2 r2 v14 l16 o2f+ o2f+ o2f+ o2f+ o2f+ o2f+ o2f+ o2f+ l4 o2f+4 o2f+4"
          "v14 [o2b4 r4 o2b8 o2b8 r4 o2g4 r4 o2g8 o2g8 r4 o3c4 r4 o3c8 o3c8 r4 l16 o2b o2b o2b o2b o2b o2b o2b o2b l4 o2b4 o2b4"
          " o2f+4 r4 o2f+8 o2f+8 r4 o2f+4 r4 o2f+8 o2f+8 r4 o2b4 r4 o2b8 o2b8 r4 l16 o2g o2g o2g o2g o2g o2g o2g o2g l4 o2g4 o2g4]2"
          "v9 [r1]4 o2b2 o2b2 o2g2 o2g2 o2b2 o2b2 l16 v12 [o2b]8 v15 [o2b]8 l4"
          "v15 o2g4 r4 o2g8 o2g8 r4 o2e4 r4 o2e8 o2e8 r4 o3d4 r4 o3d8 o3d8 r4 l16 o2b o2b o2b o2b o2b o2b o2b o2b l4 o2b4 o2b4"
          "o2f+4 r4 o2f+8 o2f+8 r4 o2f+4 r4 o2f+8 o2f+8 r4 o2g4 r4 o2g8 o2g8 r4 l16 o2b o2b o2b o2b o2b o2b o2b o2b l4 o2b4 o2b4"
          "o3c2 o3c2 o2b2 o2b2 o3c2 o3c2 l16 [o2f+]16", 0.0f },

        // Tambores de guerra
        { INST_WARDRUM, 0.75f, 0,
          "o4 v10 c2 r2 c2 r2 v12 " DRUM_BAR "v14 " DRUM_FILL
          "v14 [[" DRUM_BAR "]3 " DRUM_FILL "]4"
          "v8 [c2 r2]4 v11 [c4 r4 c4 r4]2 v13 c4 c4 c4 c4 v15 [c16]16"
          "[[" DRUM_BAR "]3 " DRUM_FILL "]2"
          "c2 c2 c2 c2 c2 c2 [c16]16", -0.15f },

        // Bumbo de orquestra
        { INST_BASSDRUM, 0.60f, 0,
          "o4 v12 c1 c1 c2 c2 c4 c4 c4 c4 v14 [c4 r4 c4 r4]16"
          "v9 [c1]4 v12 [c2 c2]2 [c4 c4 c4 c4]2 v15 [c4 c8 c8 c4 c4]8 [c2 c2]3 c1", 0.0f },

        // Caixa escura e discreta
        { INST_SNARE, 0.30f, 0,
          "o4 [r1]12 v9 [r4 c4 r4 c4]8 [r1]6 l16 v7 [c]16 v12 [c]16 v11 [r4 c4 r4 c4]8 [r1]4", 0.15f },

        // Pratos no começo das partes
        { INST_CYMBAL, 0.40f, 0,
          "o4 v13 [r1]4 c1 [r1]7 c1 [r1]15 c1 [r1]3 c1 [r1]3 c1 [r1]3", 0.2f },

        // Camada de combate: tambores de guerra extras
        { INST_WARDRUM, 0.50f, 1, "o4 v12 [[c8 c16 c16]4]40", 0.15f },

        // Glockenspiel brilhando no clímax, uma oitava acima dos violinos
        { INST_GLOCK, 0.30f, 0, "[r1]28 v10 k24 " CLIMAX "k0", 0.4f },

        // Flauta dobrando a ocarina uma oitava acima no trecho grave
        { INST_FLUTE, 0.30f, 0, "[r1]20 v10 k12 " THEME_LOW "k0", 0.3f },
    },
};
