#include "game.h"
#include <string.h>
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// Trilha sonora: músicas do autor feitas no Suno (assets/music_src/*.mp3), convertidas no
// build para OGG com o tratamento estilo N64.
//
// A lógica segue a do Ocarina of Time (estudada no Ship of Harkinian, sem copiar código):
// - Dois "tocadores" ao mesmo tempo: o PRINCIPAL (música do lugar) e o SECUNDÁRIO (inimigo).
// - Modo inimigo: o inimigo hostil mais próximo, dentro de ENEMY_FAR, liga a música de inimigo
//   (sempre do começo). O volume das duas depende da distância: perto de ENEMY_NEAR só se ouve
//   a de inimigo; longe, só a do lugar; no meio, as duas misturadas. A do lugar nunca para.
// - Modo parado: jogador parado por um tempo deixa a música do lugar mais baixa e calma,
//   com uma folga para não ficar alternando a cada passo.
// - Cada faixa continua de onde parou quando volta a tocar (pausa em vez de parar).
// - Loop suave: cada faixa é aberta duas vezes e as duas cópias se cruzam perto do fim.

// Versão do build (preenchida pelo GitHub Actions): vai no endereço das músicas
// para o navegador não reaproveitar uma cópia antiga do cache
#ifndef GAME_VERSION
#define GAME_VERSION "dev"
#endif

#define MUSIC_VOLUME   0.7f
#define RAMP_SPEED     3.0f    // volume por segundo (~10 quadros para ir de 0 a 1, como no N64)
#define LOOP_XFADE     4.0f    // segundos de mistura entre o fim e o começo da mesma faixa

// Distâncias do modo inimigo: 150 e 500 unidades do Ocarina
#define ENEMY_NEAR     OOT_DIST(150.0f)
#define ENEMY_FAR      OOT_DIST(500.0f)

// Modo parado: tempo parado para valer e volume da música do lugar nesse modo
#define STILL_TIME     1.0f    // ~30 quadros
#define STILL_GRACE    0.66f   // depois de andar ~20 quadros, paradinhas curtas não contam
#define STILL_VOLUME   0.55f

static const char *trackFiles[TRACK_COUNT] = {
    [TRACK_FIELD]  = "after_future.ogg",    // campo de Pangea
    [TRACK_BATTLE] = "paganini_fist.ogg",   // inimigo
};

typedef struct Track {
    Music deck[2];
    int active;          // cópia que está tocando agora
    float deckVolume[2]; // mistura do loop
    float volume;        // volume atual da faixa
    float target;        // volume para onde a faixa está indo
    bool loaded;
    bool paused;         // em silêncio: pausada para continuar de onde parou
} Track;

static Track tracks[TRACK_COUNT];
static TrackId areaTrack = TRACK_FIELD;
static bool enemyMode = false;
static float stillTime = 0.0f, movingTime = 0.0f;
static bool muted = false;

static void StartTrack(int i, const char *path)
{
    Track *t = &tracks[i];
    for (int d = 0; d < 2; d++)
    {
        t->deck[d] = LoadMusicStream(path);
        if (!IsMusicValid(t->deck[d])) return;
        t->deck[d].looping = false;   // a volta é feita pelo cruzamento entre as duas cópias
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

// Recomeça uma faixa do início (a música de inimigo sempre começa do começo)
static void RestartTrack(Track *t)
{
    StopMusicStream(t->deck[1 - t->active]);
    t->deckVolume[t->active] = 1.0f;
    t->deckVolume[1 - t->active] = 0.0f;
    SeekMusicStream(t->deck[t->active], 0.0f);
    PlayMusicStream(t->deck[t->active]);
    t->paused = false;
}

// Perto do fim, começa a outra cópia do início e cruza o volume entre as duas
static void UpdateLoop(Track *t)
{
    Music *cur = &t->deck[t->active];
    Music *next = &t->deck[1 - t->active];

    // Segurança: se as duas cópias pararam (aba em segundo plano, por exemplo), recomeça
    if (!IsMusicStreamPlaying(*cur) && !IsMusicStreamPlaying(*next))
    {
        RestartTrack(t);
        return;
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
}

// Modo parado com folga: só vale depois de STILL_TIME parado, e uma paradinha curta
// logo depois de andar não conta
static bool StillMode(bool playerStill, float dt)
{
    if (playerStill) stillTime += dt;
    else
    {
        if (stillTime >= STILL_TIME) movingTime = 0.0f;
        stillTime = 0.0f;
        movingTime += dt;
    }
    if (!playerStill) return false;
    if (stillTime < STILL_TIME && movingTime > STILL_GRACE) return false;
    return stillTime >= STILL_TIME;
}

void Music_Update(float enemyDist, bool playerStill, float dt)
{
    // M ou Select liga e desliga a música
    if (IsKeyPressed(KEY_M) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT)))
        muted = !muted;

    // Modo inimigo: algum inimigo hostil dentro do alcance
    bool enemyNear = enemyDist >= 0.0f && enemyDist < ENEMY_FAR;
    if (enemyNear && !enemyMode && tracks[TRACK_BATTLE].loaded) RestartTrack(&tracks[TRACK_BATTLE]);
    enemyMode = enemyNear;

    // Volume da música de inimigo pela distância (perto = cheia), e a do lugar no inverso
    float enemyVol = 0.0f;
    if (enemyMode) enemyVol = (ENEMY_FAR - fmaxf(enemyDist, ENEMY_NEAR))/(ENEMY_FAR - ENEMY_NEAR);
    float areaVol = 1.0f - enemyVol;
    if (StillMode(playerStill, dt) && !enemyMode) areaVol *= STILL_VOLUME;

    for (int i = 0; i < TRACK_COUNT; i++) tracks[i].target = 0.0f;
    tracks[areaTrack].target = areaVol*MUSIC_VOLUME;
    tracks[TRACK_BATTLE].target = enemyVol*MUSIC_VOLUME;

    for (int i = 0; i < TRACK_COUNT; i++)
    {
        Track *t = &tracks[i];
        if (!t->loaded) continue;

        if (t->volume < t->target) t->volume = fminf(t->target, t->volume + RAMP_SPEED*dt);
        else t->volume = fmaxf(t->target, t->volume - RAMP_SPEED*dt);

        // Faixa em silêncio fica pausada e depois continua de onde parou
        if (!t->paused && t->volume <= 0.0f && t->target <= 0.0f)
        {
            for (int d = 0; d < 2; d++) PauseMusicStream(t->deck[d]);
            t->paused = true;
        }
        if (t->paused)
        {
            if (t->target <= 0.0f) continue;
            for (int d = 0; d < 2; d++) if (t->deckVolume[d] > 0.0f) ResumeMusicStream(t->deck[d]);
            t->paused = false;
        }

        UpdateLoop(t);
        for (int d = 0; d < 2; d++)
        {
            SetMusicVolume(t->deck[d], muted ? 0.0f : t->volume*t->deckVolume[d]);
            if (IsMusicStreamPlaying(t->deck[d])) UpdateMusicStream(t->deck[d]);
        }
    }
}
