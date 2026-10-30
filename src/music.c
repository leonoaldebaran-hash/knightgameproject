#include "game.h"
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// Nome do banco de instrumentos (o build da web troca por um nome com versão)
#ifndef SOUNDBANK_FILE
#define SOUNDBANK_FILE "soundbank.bin"
#endif

#define BATTLE_HOLD  2.5f   // segundos que a camada de combate continua depois da luta

static float battleHold = 0.0f;
static unsigned char *bankData = NULL;   // fica na memória enquanto o jogo roda

static void StartWithBank(const char *path)
{
    int size = 0;
    bankData = LoadFileData(path, &size);
    if (bankData && Synth_LoadBank(bankData, size)) Synth_Play(&SONG_FIELD);
    else TraceLog(LOG_WARNING, "MUSIC: banco de instrumentos invalido: %s", path);
}

#if defined(PLATFORM_WEB)
// No navegador o banco é baixado depois que o jogo abre, para não atrasar o início
static void OnBankDownloaded(const char *file) { StartWithBank(file); }
static void OnBankFailed(const char *file) { TraceLog(LOG_WARNING, "MUSIC: falha ao baixar %s", file); }
#endif

void Music_Init(void)
{
    Synth_Init();
#if defined(PLATFORM_WEB)
    emscripten_async_wget(SOUNDBANK_FILE, "/" SOUNDBANK_FILE, OnBankDownloaded, OnBankFailed);
#else
    StartWithBank("assets/" SOUNDBANK_FILE);
#endif
}

void Music_Update(bool inBattle, float dt)
{
    // M ou Select liga e desliga a música
    if (IsKeyPressed(KEY_M) || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT)))
        Synth_ToggleMute();

    // A mesma música ganha a camada de combate quando um inimigo persegue o jogador,
    // e ela sai devagar um pouco depois da luta, como no Ocarina
    if (inBattle) battleHold = BATTLE_HOLD;
    else if (battleHold > 0.0f) battleHold -= dt;

    Synth_SetIntensity((battleHold > 0.0f) ? 1.0f : 0.0f);
}
