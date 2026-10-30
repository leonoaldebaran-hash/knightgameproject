#include "game.h"
#include <stdlib.h>

// Efeitos sonoros sintetizados na hora (sem arquivos), no espírito dos sons curtos do N64
#define SFX_RATE  22050
#define MAX_SFX_SAMPLES (SFX_RATE/2)   // até meio segundo por efeito

static Sound sounds[SFX_COUNT];
static short buffer[MAX_SFX_SAMPLES];

static float Noise(void)
{
    return (float)rand()/(float)RAND_MAX*2.0f - 1.0f;
}

static Sound MakeSound(int samples)
{
    Wave w = { .frameCount = (unsigned int)samples, .sampleRate = SFX_RATE, .sampleSize = 16, .channels = 1, .data = buffer };
    return LoadSoundFromWave(w);
}

static short ToSample(float v)
{
    return (short)(Clamp(v, -1.0f, 1.0f)*32000.0f);
}

// Espada cortando o ar: ruído com um filtro que vai fechando ("swish")
static Sound SynthSwing(void)
{
    int n = (int)(0.14f*SFX_RATE);
    float lp = 0.0f;
    for (int i = 0; i < n; i++)
    {
        float t = (float)i/n;
        float cutoff = Lerp(0.6f, 0.05f, t);
        lp += (Noise() - lp)*cutoff;
        float env = sinf(t*PI);   // sobe e desce
        buffer[i] = ToSample(lp*env*0.9f);
    }
    return MakeSound(n);
}

// Lâmina acertando: batida grave curta com um estalo por cima
static Sound SynthHit(void)
{
    int n = (int)(0.12f*SFX_RATE);
    float phase = 0.0f;
    for (int i = 0; i < n; i++)
    {
        float t = (float)i/n;
        float freq = Lerp(180.0f, 60.0f, t);
        phase += 2.0f*PI*freq/SFX_RATE;
        float env = (1.0f - t)*(1.0f - t);
        float thump = sinf(phase)*env;
        float crack = Noise()*expf(-t*30.0f)*0.6f;
        buffer[i] = ToSample((thump + crack)*0.9f);
    }
    return MakeSound(n);
}

// Escudo defendendo: metal ressoando (senos fora de harmonia com decaimento)
static Sound SynthBlock(void)
{
    const float freqs[4] = { 1180.0f, 1870.0f, 2630.0f, 3410.0f };
    int n = (int)(0.35f*SFX_RATE);
    for (int i = 0; i < n; i++)
    {
        float t = (float)i/SFX_RATE;
        float v = 0.0f;
        for (int k = 0; k < 4; k++) v += sinf(2.0f*PI*freqs[k]*t)*expf(-t*(8.0f + k*4.0f));
        v += Noise()*expf(-t*60.0f)*0.5f;
        buffer[i] = ToSample(v*0.3f);
    }
    return MakeSound(n);
}

// Mateo levando dano: onda quadrada descendo de tom
static Sound SynthHurt(void)
{
    int n = (int)(0.22f*SFX_RATE);
    float phase = 0.0f;
    for (int i = 0; i < n; i++)
    {
        float t = (float)i/n;
        phase += Lerp(520.0f, 180.0f, t)/SFX_RATE;
        float square = (fmodf(phase, 1.0f) < 0.5f) ? 1.0f : -1.0f;
        buffer[i] = ToSample(square*(1.0f - t)*0.35f);
    }
    return MakeSound(n);
}

// Aviso do soldado levantando a espada: um "tim" metálico curto
static Sound SynthAlert(void)
{
    int n = (int)(0.18f*SFX_RATE);
    for (int i = 0; i < n; i++)
    {
        float t = (float)i/SFX_RATE;
        float v = sinf(2.0f*PI*1500.0f*t) + 0.5f*sinf(2.0f*PI*2250.0f*t);
        buffer[i] = ToSample(v*expf(-t*25.0f)*0.35f);
    }
    return MakeSound(n);
}

void Sfx_Init(void)
{
    sounds[SFX_SWING] = SynthSwing();
    sounds[SFX_HIT] = SynthHit();
    sounds[SFX_BLOCK] = SynthBlock();
    sounds[SFX_HURT] = SynthHurt();
    sounds[SFX_ALERT] = SynthAlert();
}

void Sfx_Play(SfxId id)
{
    // Pequena variação de tom a cada vez, para não soar repetitivo
    SetSoundPitch(sounds[id], 0.94f + 0.12f*(float)rand()/(float)RAND_MAX);
    PlaySound(sounds[id]);
}
