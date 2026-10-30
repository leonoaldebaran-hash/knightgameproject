#include "game.h"

#define BATTLE_HOLD  2.5f   // segundos que a camada de combate continua depois da luta

static float battleHold = 0.0f;

void Music_Init(void)
{
    Synth_Init();
    Synth_Play(&SONG_FIELD);
}

void Music_Update(bool inBattle, float dt)
{
    // A mesma música do campo ganha tambores e metais quando um inimigo te persegue,
    // e eles saem devagar um pouco depois da luta, como no Ocarina
    if (inBattle) battleHold = BATTLE_HOLD;
    else if (battleHold > 0.0f) battleHold -= dt;

    Synth_SetIntensity((battleHold > 0.0f) ? 1.0f : 0.0f);
}
