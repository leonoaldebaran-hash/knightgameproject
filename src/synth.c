#include "game.h"
#include <string.h>

// Música sequenciada no estilo do N64: as notas ficam escritas no código (em MML)
// e são tocadas ao vivo por instrumentos sintetizados, nota por nota.

#define SYNTH_RATE      32000   // taxa típica dos cartuchos de N64
#define TICKS_PER_BEAT  48      // resolução: 48 divisões por semínima
#define MAX_CHANNELS    8
#define MAX_EVENTS      1024
#define MAX_VOICES      28
#define KS_MAX          1200    // memória da corda dedilhada (nota mais grave ~27 Hz)
#define BUFFER_FRAMES   1024

typedef struct NoteEvent { int start, length, midi; float velocity; } NoteEvent;

typedef struct Channel {
    int instrument;
    float volume;
    int layer;
    NoteEvent events[MAX_EVENTS];
    int count;
    int next;
} Channel;

typedef struct Voice {
    bool active;
    int instrument, channel;
    float freq, velocity;
    float time, holdTime;     // segundos desde o início / duração da nota
    float released;           // segundos desde a soltura (< 0 = ainda segurando)
    float releaseLevel;
    float phase[3];
    float lp, lp2;
    float ks[KS_MAX];
    int ksLen, ksPos;
    int drum;
} Voice;

typedef struct InstrumentDef { float attack, decay, sustain, release, gain; } InstrumentDef;

static const InstrumentDef defs[INST_COUNT] = {
    [INST_STRINGS] = { 0.08f, 0.30f, 0.80f, 0.35f, 0.55f },
    [INST_FLUTE]   = { 0.05f, 0.10f, 0.90f, 0.15f, 0.50f },
    [INST_BRASS]   = { 0.03f, 0.25f, 0.85f, 0.15f, 0.65f },
    [INST_HARP]    = { 0.00f, 0.00f, 1.00f, 0.25f, 0.70f },
    [INST_BASS]    = { 0.01f, 0.30f, 0.60f, 0.08f, 0.70f },
    [INST_CHOIR]   = { 0.25f, 0.30f, 0.85f, 0.50f, 0.45f },
    [INST_BELL]    = { 0.00f, 0.00f, 1.00f, 0.80f, 0.40f },
    [INST_DRUMS]   = { 0.00f, 0.00f, 1.00f, 0.05f, 0.80f },
    [INST_TIMPANI] = { 0.00f, 0.00f, 1.00f, 0.30f, 0.90f },
    [INST_STACCATO]= { 0.005f, 0.08f, 0.45f, 0.06f, 0.50f },
};

static AudioStream stream;
static Channel channels[MAX_CHANNELS];
static int channelCount = 0;
static Voice voices[MAX_VOICES];
static volatile bool playing = false;
static float bpm = 120.0f;
static int songLength = 0;          // em ticks
static int tick = 0;
static double tickAcc = 0.0;
static unsigned int noiseState = 22222;
static float masterVolume = 0.8f;
static bool muted = false;
static float layerTarget[2] = { 1.0f, 0.0f }, layerGain[2] = { 1.0f, 0.0f };

// Eco simples (2 filtros pente + 1 passa-tudo): a "sala" típica das trilhas de N64
static float combA[860], combB[930], allp[400];
static int combAPos, combBPos, allpPos;
static float combALp, combBLp;

static float Noise(void)
{
    noiseState ^= noiseState << 13;
    noiseState ^= noiseState >> 17;
    noiseState ^= noiseState << 5;
    return (float)(noiseState & 0xFFFF)/32768.0f - 1.0f;
}

// ---------------------------------------------------------------------------
// MML: c d e f g a b (+ ou # sustenido, - bemol), r pausa, número = duração
// (4 = semínima, 8 = colcheia...), "." pontua, o4 oitava, < > desce/sobe oitava,
// l8 duração padrão, v12 força (0 a 15), [ ... ]3 repete 3 vezes.
// ---------------------------------------------------------------------------
typedef struct MmlState { int octave, length, tick; float velocity; Channel *ch; } MmlState;

static int ReadNumber(const char **s)
{
    int n = 0;
    bool any = false;
    while (**s >= '0' && **s <= '9') { n = n*10 + (**s - '0'); (*s)++; any = true; }
    return any ? n : -1;
}

static int ReadLength(const char **s, int defaultTicks)
{
    int n = ReadNumber(s);
    int ticks = (n > 0) ? (TICKS_PER_BEAT*4)/n : defaultTicks;
    int add = ticks/2;
    while (**s == '.') { ticks += add; add /= 2; (*s)++; }
    return ticks;
}

static void ParseMml(const char *s, const char *end, MmlState *st)
{
    static const int semis[7] = { 9, 11, 0, 2, 4, 5, 7 };   // a b c d e f g

    while (s < end && *s)
    {
        char c = *s++;
        if (c >= 'a' && c <= 'g')
        {
            int semi = semis[c - 'a'];
            if (*s == '+' || *s == '#') { semi++; s++; }
            else if (*s == '-') { semi--; s++; }
            int ticks = ReadLength(&s, st->length);
            if (st->ch->count < MAX_EVENTS)
                st->ch->events[st->ch->count++] = (NoteEvent){ st->tick, ticks, (st->octave + 1)*12 + semi, st->velocity };
            st->tick += ticks;
        }
        else if (c == 'r') st->tick += ReadLength(&s, st->length);
        else if (c == 'o') st->octave = ReadNumber(&s);
        else if (c == '<') st->octave--;
        else if (c == '>') st->octave++;
        else if (c == 'l') st->length = ReadLength(&s, st->length);
        else if (c == 'v') st->velocity = ReadNumber(&s)/15.0f;
        else if (c == '[')
        {
            // Acha o ']' correspondente e repete o trecho
            int depth = 1;
            const char *close = s;
            while (close < end && *close && depth > 0)
            {
                if (*close == '[') depth++;
                else if (*close == ']') depth--;
                if (depth > 0) close++;
            }
            const char *after = close + 1;
            int times = ReadNumber(&after);
            if (times < 1) times = 2;
            for (int k = 0; k < times; k++) ParseMml(s, close, st);
            s = after;
        }
    }
}

// ---------------------------------------------------------------------------
// Vozes e instrumentos
// ---------------------------------------------------------------------------
static float MidiToFreq(int midi)
{
    return 440.0f*powf(2.0f, (midi - 69)/12.0f);
}

static void NoteOn(int channel, const NoteEvent *e)
{
    // Usa uma voz livre; se não houver, rouba a mais antiga
    Voice *v = NULL;
    for (int i = 0; i < MAX_VOICES && !v; i++) if (!voices[i].active) v = &voices[i];
    if (!v)
    {
        v = &voices[0];
        for (int i = 1; i < MAX_VOICES; i++) if (voices[i].time > v->time) v = &voices[i];
    }

    Channel *ch = &channels[channel];
    memset(v->phase, 0, sizeof(v->phase));
    v->active = true;
    v->instrument = ch->instrument;
    v->channel = channel;
    v->freq = MidiToFreq(e->midi);
    v->velocity = e->velocity*ch->volume;
    v->time = 0.0f;
    v->holdTime = e->length*60.0f/(bpm*TICKS_PER_BEAT);
    v->released = -1.0f;
    v->lp = v->lp2 = 0.0f;
    v->drum = e->midi % 12;   // na bateria: c bumbo, d caixa, e/f chimbal, g tom, a prato

    if (v->instrument == INST_HARP)
    {
        // Corda dedilhada (Karplus-Strong): começa com ruído e vai se "afinando"
        v->ksLen = (int)Clamp(SYNTH_RATE/v->freq, 2, KS_MAX);
        v->ksPos = 0;
        for (int i = 0; i < v->ksLen; i++) v->ks[i] = Noise();
    }
    if (v->instrument == INST_DRUMS) v->holdTime = (v->drum >= 9) ? 1.6f : 0.4f;   // o prato soa longo
    if (v->instrument == INST_TIMPANI) v->holdTime = fmaxf(v->holdTime, 1.2f);
}

static float Envelope(Voice *v)
{
    const InstrumentDef *d = &defs[v->instrument];
    float t = v->time, level;
    if (t < d->attack) level = t/d->attack;
    else if (t < d->attack + d->decay) level = Lerp(1.0f, d->sustain, (t - d->attack)/d->decay);
    else level = d->sustain;

    if (v->released < 0.0f) return level;
    return v->releaseLevel*fmaxf(0.0f, 1.0f - v->released/d->release);
}

static float Advance(float *phase, float freq)
{
    *phase += freq/SYNTH_RATE;
    if (*phase >= 1.0f) *phase -= floorf(*phase);
    return *phase;
}

static float RenderVoice(Voice *v)
{
    float t = v->time;
    float vib = 1.0f + 0.004f*sinf(2.0f*PI*5.0f*t)*fminf(1.0f, t/0.3f);   // vibrato que entra devagar
    float out = 0.0f;

    switch (v->instrument)
    {
        case INST_STRINGS:   // duas ondas dente de serra levemente desafinadas, filtradas
        {
            float a = Advance(&v->phase[0], v->freq*vib*0.997f);
            float b = Advance(&v->phase[1], v->freq*vib*1.003f);
            float saw = (a + b) - 1.0f;
            v->lp += (saw - v->lp)*0.22f;
            out = v->lp;
        } break;

        case INST_FLUTE:     // senoide com um pouco de sopro
        {
            float p = Advance(&v->phase[0], v->freq*vib);
            float tone = sinf(2.0f*PI*p) + 0.15f*sinf(4.0f*PI*p);
            v->lp += (Noise()*0.08f - v->lp)*0.1f;
            out = tone + v->lp;
        } break;

        case INST_BRASS:     // naipe de metais: 3 dentes de serra desafinadas, filtro que "rasga" no ataque
        {
            const float detune[3] = { 0.994f, 1.0f, 1.006f };
            float saw = 0.0f;
            for (int k = 0; k < 3; k++) saw += 2.0f*Advance(&v->phase[k], v->freq*vib*detune[k]) - 1.0f;
            saw *= 0.45f;
            float cutoff = 0.16f + 0.45f*fminf(1.0f, t/0.04f)*expf(-t*3.0f);
            v->lp += (saw - v->lp)*cutoff;
            v->lp2 += (v->lp - v->lp2)*cutoff;
            out = tanhf(v->lp2*2.2f);   // leve saturação: som de metal soprado com força
        } break;

        case INST_STACCATO:  // cordas curtas: dente de serra brilhante
        {
            float a = Advance(&v->phase[0], v->freq*0.998f);
            float b = Advance(&v->phase[1], v->freq*1.002f);
            float saw = (a + b) - 1.0f;
            v->lp += (saw - v->lp)*0.35f;
            out = v->lp;
        } break;

        case INST_TIMPANI:   // tímpano: tom grave que cai um pouco, com batida de ruído
        {
            float p = Advance(&v->phase[0], v->freq*(1.0f + 0.08f*expf(-t*20.0f)));
            float q = Advance(&v->phase[1], v->freq*1.5f);
            float body = (sinf(2.0f*PI*p) + 0.3f*sinf(2.0f*PI*q))*expf(-t*2.2f);
            out = body*1.3f + Noise()*expf(-t*40.0f)*0.4f;
        } break;

        case INST_HARP:
        {
            int next = (v->ksPos + 1) % v->ksLen;
            out = v->ks[v->ksPos];
            v->ks[v->ksPos] = 0.5f*(v->ks[v->ksPos] + v->ks[next])*0.997f;
            v->ksPos = next;
        } break;

        case INST_BASS:      // triângulo com um pouco de onda quadrada
        {
            float p = Advance(&v->phase[0], v->freq);
            out = (4.0f*fabsf(p - 0.5f) - 1.0f)*0.8f + ((p < 0.5f) ? 0.15f : -0.15f);
        } break;

        case INST_CHOIR:     // três vozes de senoides com harmônicos
        {
            const float detune[3] = { 0.995f, 1.0f, 1.005f };
            for (int k = 0; k < 3; k++)
            {
                float p = Advance(&v->phase[k], v->freq*vib*detune[k]);
                out += sinf(2.0f*PI*p) + 0.4f*sinf(4.0f*PI*p) + 0.2f*sinf(6.0f*PI*p);
            }
            out *= 0.3f;
        } break;

        case INST_BELL:      // síntese FM: som metálico de sino
        {
            float c = Advance(&v->phase[0], v->freq);
            float m = Advance(&v->phase[1], v->freq*3.5f);
            out = sinf(2.0f*PI*c + 2.0f*sinf(2.0f*PI*m)*expf(-t*3.0f))*expf(-t*2.5f);
        } break;

        case INST_DRUMS:
            if (v->drum == 0)          // bumbo: senoide caindo de tom
            {
                float p = Advance(&v->phase[0], Lerp(45.0f, 150.0f, expf(-t*25.0f)));
                out = sinf(2.0f*PI*p)*expf(-t*12.0f)*1.4f;
            }
            else if (v->drum <= 3)     // caixa: ruído + tom
            {
                float p = Advance(&v->phase[0], 185.0f);
                out = (Noise()*0.8f + sinf(2.0f*PI*p)*0.5f)*expf(-t*18.0f);
            }
            else if (v->drum <= 5)     // chimbal: ruído agudo bem curto
            {
                float n = Noise();
                v->lp += (n - v->lp)*0.5f;
                out = (n - v->lp)*expf(-t*45.0f)*0.6f;
            }
            else if (v->drum <= 8)     // tom: tambor grave de guerra
            {
                float p = Advance(&v->phase[0], Lerp(80.0f, 130.0f, expf(-t*15.0f)));
                out = (sinf(2.0f*PI*p)*1.2f + Noise()*0.2f*expf(-t*30.0f))*expf(-t*7.0f);
            }
            else                       // prato: ruído brilhante que soa longo
            {
                float n = Noise();
                v->lp += (n - v->lp)*0.6f;
                out = (n - v->lp)*expf(-t*2.5f)*0.55f;
            }
            break;
    }

    return out*Envelope(v)*defs[v->instrument].gain*v->velocity;
}

static float Reverb(float in)
{
    float a = combA[combAPos];
    combALp += (a - combALp)*0.4f;
    combA[combAPos] = in + combALp*0.78f;
    combAPos = (combAPos + 1) % (int)(sizeof(combA)/sizeof(float));

    float b = combB[combBPos];
    combBLp += (b - combBLp)*0.4f;
    combB[combBPos] = in + combBLp*0.76f;
    combBPos = (combBPos + 1) % (int)(sizeof(combB)/sizeof(float));

    float sum = (a + b)*0.5f;
    float buf = allp[allpPos];
    float outp = -sum + buf;
    allp[allpPos] = sum + buf*0.5f;
    allpPos = (allpPos + 1) % (int)(sizeof(allp)/sizeof(float));
    return outp;
}

// Avança a partitura um tick: solta notas novas e reinicia no fim (loop)
static void StepTick(void)
{
    for (int c = 0; c < channelCount; c++)
    {
        Channel *ch = &channels[c];
        while (ch->next < ch->count && ch->events[ch->next].start == tick)
        {
            NoteOn(c, &ch->events[ch->next]);
            ch->next++;
        }
    }
    tick++;
    if (tick >= songLength)
    {
        tick = 0;
        for (int c = 0; c < channelCount; c++) channels[c].next = 0;
    }
}

// Roda na thread de áudio: gera cada amostra na hora
static void SynthCallback(void *buffer, unsigned int frames)
{
    short *out = (short *)buffer;
    double samplesPerTick = SYNTH_RATE*60.0/(bpm*TICKS_PER_BEAT);

    for (unsigned int i = 0; i < frames; i++)
    {
        float mix = 0.0f;
        if (playing)
        {
            tickAcc += 1.0;
            while (tickAcc >= samplesPerTick) { tickAcc -= samplesPerTick; StepTick(); }

            for (int l = 0; l < 2; l++) layerGain[l] += (layerTarget[l] - layerGain[l])*0.0002f;

            for (int k = 0; k < MAX_VOICES; k++)
            {
                Voice *v = &voices[k];
                if (!v->active) continue;

                float s = RenderVoice(v)*layerGain[channels[v->channel].layer];
                mix += s;

                v->time += 1.0f/SYNTH_RATE;
                if (v->released < 0.0f && v->time >= v->holdTime)
                {
                    v->releaseLevel = Envelope(v);
                    v->released = 0.0f;
                }
                else if (v->released >= 0.0f)
                {
                    v->released += 1.0f/SYNTH_RATE;
                    if (v->released >= defs[v->instrument].release) v->active = false;
                }
            }
            mix += Reverb(mix*0.5f)*0.35f;
        }
        mix = tanhf(mix*(muted ? 0.0f : masterVolume)*0.6f);   // limita o volume sem estourar
        out[i] = (short)(mix*30000.0f);
    }
}

void Synth_Init(void)
{
    SetAudioStreamBufferSizeDefault(BUFFER_FRAMES);
    stream = LoadAudioStream(SYNTH_RATE, 16, 1);
    SetAudioStreamCallback(stream, SynthCallback);
    PlayAudioStream(stream);
}

void Synth_Play(const Song *song)
{
    playing = false;
    memset(voices, 0, sizeof(voices));

    bpm = song->bpm;
    channelCount = song->channelCount;
    songLength = 0;
    for (int c = 0; c < channelCount; c++)
    {
        const SongChannel *sc = &song->channels[c];
        Channel *ch = &channels[c];
        ch->instrument = sc->instrument;
        ch->volume = sc->volume;
        ch->layer = sc->layer;
        ch->count = 0;
        ch->next = 0;

        MmlState st = { .octave = 4, .length = TICKS_PER_BEAT, .tick = 0, .velocity = 0.8f, .ch = ch };
        ParseMml(sc->mml, sc->mml + strlen(sc->mml), &st);
        if (st.tick > songLength) songLength = st.tick;
    }

    tick = 0;
    tickAcc = 0.0;
    playing = true;
}

void Synth_SetIntensity(float amount)
{
    layerTarget[1] = Clamp(amount, 0.0f, 1.0f);
}

void Synth_SetVolume(float volume)
{
    masterVolume = volume;
}

bool Synth_ToggleMute(void)
{
    muted = !muted;
    return muted;
}
