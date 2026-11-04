#include "game.h"
#include <string.h>
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// Trilha sonora: músicas do autor feitas no Suno (assets/music_src/*.mp3), convertidas no
// build para OGG com o tratamento estilo N64. Cada lugar tem sua faixa, e a de combate
// entra por cima quando um inimigo persegue o jogador.
//
// Loop suave: cada faixa é aberta duas vezes ("dois toca-discos"). Perto do fim, a segunda
// cópia recomeça do início enquanto a primeira some, então a volta não tem corte.

// Versão do build (preenchida pelo GitHub Actions): vai no endereço das músicas
// para o navegador não reaproveitar uma cópia antiga do cache
#ifndef GAME_VERSION
#define GAME_VERSION "dev"
#endif

#define MUSIC_VOLUME   0.7f
#define FADE_SPEED     1.2f   // volume por segundo na troca de faixa
#define LOOP_XFADE     4.0f   // segundos de mistura entre o fim e o começo da mesma faixa
#define BATTLE_HOLD    2.5f   // segundos que a música de combate continua depois da luta

static const char *trackFiles[TRACK_COUNT] = {
    [TRACK_FIELD]  = "after_future.ogg",    // campo de Pangea
    [TRACK_BATTLE] = "paganini_fist.ogg",   // combate
};

typedef struct Track {
    Music deck[2];
    int active;          // toca-discos que está tocando agora
    float deckVolume[2]; // mistura do loop
    float volume;        // volume da faixa (troca de lugar / combate)
    bool loaded;
} Track;

static Track tracks[TRACK_COUNT];
static TrackId areaTrack = TRACK_FIELD;
static float battleHold = 0.0f;
static bool muted = false;

static void StartTrack(int i, const char *path)
{
    Track *t = &tracks[i];
    for (int d = 0; d < 2; d++)
    {
        t->deck[d] = LoadMusicStream(path);
        if (!IsMusicValid(t->deck[d])) return;
        t->deck[d].looping = false;   // a volta é feita pelo cruzamento entre os dois
    }
    t->active = 0;
    t->deckVolume[0] = 1.0f;
    t->deckVolume[1] = 0.0f;
    PlayMusicStream(t->deck[0]);
    t->loaded = true;
}

#if defined(PLATFORM_WEB)
// No navegador as músicas são baixadas depois que o jogo abre, para não atrasar o início
static void OnTrackDownloaded(const char *file)
{
    for (int i = 0; i < TRACK_COUNT; i++)
        if (strstr(file, trackFiles[i])) StartTrack(i, file);
}

static void OnTrackFailed(const char *file)
{
    TraceLog(LOG_WARNING, "MUSIC: nao foi possivel baixar %s", file);
}
#endif

void Music_Init(void)
{
    for (int i = 0; i < TRACK_COUNT; i++)
    {
#if defined(PLATFORM_WEB)
        emscripten_async_wget(TextFormat("music/%s?v=%s", trackFiles[i], GAME_VERSION), TextFormat("/%s", trackFiles[i]), OnTrackDownloaded, OnTrackFailed);
#else
        StartTrack(i, TextFormat("assets/music/%s", trackFiles[i]));
#endif
    }
}

void Music_SetArea(TrackId track)
{
    areaTrack = track;
}

// Perto do fim, começa a outra cópia do início e cruza o volume entre as duas
static void UpdateLoop(Track *t, float dt)
{
    Music *cur = &t->deck[t->active];
    Music *next = &t->deck[1 - t->active];

    // Segurança: se as duas cópias pararam (aba em segundo plano, por exemplo), recomeça
    if (!IsMusicStreamPlaying(*cur) && !IsMusicStreamPlaying(*next))
    {
        SeekMusicStream(*cur, 0.0f);
        PlayMusicStream(*cur);
        t->deckVolume[t->active] = 1.0f;
        t->deckVolume[1 - t->active] = 0.0f;
    }

    float remaining = GetMusicTimeLength(*cur) - GetMusicTimePlayed(*cur);

    if (remaining <= LOOP_XFADE && !IsMusicStreamPlaying(*next))
    {
        SeekMusicStream(*next, 0.0f);
        PlayMusicStream(*next);
    }

    if (IsMusicStreamPlaying(*next))
    {
        float k = Clamp(1.0f - remaining/LOOP_XFADE, 0.0f, 1.0f);
        t->deckVolume[t->active] = 1.0f - k;
        t->deckVolume[1 - t->active] = k;
        if (k >= 1.0f || !IsMusicStreamPlaying(*cur))
        {
            StopMusicStream(*cur);
            t->deckVolume[t->active] = 0.0f;
            t->deckVolume[1 - t->active] = 1.0f;
            t->active = 1 - t->active;
        }
    }
    (void)dt;
}

void Music_Update(bool inBattle, float dt)
{
    // M ou Select liga e desliga a música
    if (IsKeyPressed(KEY_M) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT)))
        muted = !muted;

    // Segura a música de combate um pouco depois da luta, para não ficar trocando toda hora
    if (inBattle) battleHold = BATTLE_HOLD;
    else if (battleHold > 0.0f) battleHold -= dt;

    TrackId wanted = (battleHold > 0.0f) ? TRACK_BATTLE : areaTrack;

    for (int i = 0; i < TRACK_COUNT; i++)
    {
        Track *t = &tracks[i];
        if (!t->loaded) continue;

        // A música de combate sempre começa do início, como no N64
        if (i == TRACK_BATTLE && wanted == TRACK_BATTLE && t->volume < 0.01f)
        {
            StopMusicStream(t->deck[1 - t->active]);
            t->deckVolume[t->active] = 1.0f;
            t->deckVolume[1 - t->active] = 0.0f;
            SeekMusicStream(t->deck[t->active], 0.0f);
            if (!IsMusicStreamPlaying(t->deck[t->active])) PlayMusicStream(t->deck[t->active]);
        }

        float target = (i == (int)wanted) ? MUSIC_VOLUME : 0.0f;
        if (t->volume < target) t->volume = fminf(target, t->volume + FADE_SPEED*dt);
        else t->volume = fmaxf(target, t->volume - FADE_SPEED*dt);

        UpdateLoop(t, dt);
        for (int d = 0; d < 2; d++)
        {
            SetMusicVolume(t->deck[d], muted ? 0.0f : t->volume*t->deckVolume[d]);
            if (IsMusicStreamPlaying(t->deck[d])) UpdateMusicStream(t->deck[d]);
        }
    }
}
