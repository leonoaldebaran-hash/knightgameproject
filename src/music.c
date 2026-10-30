#include "game.h"
#include <string.h>
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define MUSIC_VOLUME   0.7f
#define FADE_SPEED     1.2f   // volume por segundo na troca de faixa
#define BATTLE_HOLD    2.5f   // segundos que a música de combate continua depois da luta

typedef enum { TRACK_FIELD, TRACK_BATTLE, TRACK_COUNT } TrackId;

// Faixas compostas pelo autor do jogo (convertidas para OGG no build)
static const char *trackFiles[TRACK_COUNT] = {
    "after_future.ogg",    // campo de Pangea
    "paganini_fist.ogg",   // combate
};

static Music tracks[TRACK_COUNT];
static bool loaded[TRACK_COUNT];
static float volume[TRACK_COUNT];
static TrackId current = TRACK_FIELD;
static float battleHold = 0.0f;

static void StartTrack(int i, const char *path)
{
    tracks[i] = LoadMusicStream(path);
    if (!IsMusicValid(tracks[i])) return;
    tracks[i].looping = true;
    SetMusicVolume(tracks[i], 0.0f);
    PlayMusicStream(tracks[i]);
    loaded[i] = true;
}

#if defined(PLATFORM_WEB)
// No navegador as músicas são baixadas só depois que o jogo abre, para não atrasar o início
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
        emscripten_async_wget(TextFormat("music/%s", trackFiles[i]), TextFormat("/%s", trackFiles[i]), OnTrackDownloaded, OnTrackFailed);
#else
        StartTrack(i, TextFormat("assets/music/%s", trackFiles[i]));
#endif
    }
}

void Music_Update(bool inBattle, float dt)
{
    // Segura a música de combate um pouco depois da luta, para não ficar trocando toda hora
    if (inBattle) battleHold = BATTLE_HOLD;
    else if (battleHold > 0.0f) battleHold -= dt;

    TrackId wanted = (battleHold > 0.0f) ? TRACK_BATTLE : TRACK_FIELD;

    // A música de combate sempre começa do início, como no N64
    if (wanted == TRACK_BATTLE && current != TRACK_BATTLE && loaded[TRACK_BATTLE] && volume[TRACK_BATTLE] < 0.01f)
        SeekMusicStream(tracks[TRACK_BATTLE], 0.0f);
    current = wanted;

    for (int i = 0; i < TRACK_COUNT; i++)
    {
        if (!loaded[i]) continue;
        float target = (i == (int)current) ? MUSIC_VOLUME : 0.0f;
        if (volume[i] < target) volume[i] = fminf(target, volume[i] + FADE_SPEED*dt);
        else volume[i] = fmaxf(target, volume[i] - FADE_SPEED*dt);
        SetMusicVolume(tracks[i], volume[i]);
        UpdateMusicStream(tracks[i]);
    }
}
