#include "game.h"
#include <stdlib.h>
#include <string.h>

// Música sequenciada no estilo do N64: a partitura fica escrita no código (em MML)
// e é tocada ao vivo, nota por nota, por instrumentos de orquestra gravados
// (banco em assets/soundbank.bin, montado por tools/build_soundbank.py).

#define SYNTH_RATE      32000   // taxa típica dos cartuchos de N64
#define TICKS_PER_BEAT  48      // resolução: 48 divisões por semínima
#define MAX_CHANNELS    24
#define MAX_EVENTS      2048
#define MAX_VOICES      64
#define MAX_BANK_SAMPLES 16
#define BUFFER_FRAMES   1024

typedef struct NoteEvent { int start, length, midi; float velocity; } NoteEvent;

typedef struct Channel {
    int instrument;
    float volume, panL, panR;
    int layer;
    NoteEvent events[MAX_EVENTS];
    int count;
    int next;
} Channel;

typedef struct BankSample { float root; int frames; const short *data; } BankSample;
typedef struct BankInstrument { bool unpitched; int count; BankSample samples[MAX_BANK_SAMPLES]; } BankInstrument;

typedef struct Voice {
    bool active;
    int instrument, channel;
    const BankSample *sample;
    double pos, step;
    float freq, velocity;
    float time, holdTime;     // segundos desde o início / duração da nota
    float released;           // segundos desde a soltura (< 0 = ainda segurando)
    float releaseLevel;
    float phase[4];           // coral sintetizado
    float bandA[3], lowA[3];
    float dark;               // filtro que escurece o timbre
} Voice;

// Ataque, soltura, volume, se a nota toca até o fim da gravação (golpes, percussão)
// e o quanto o timbre é escurecido (0 = sem filtro; quanto menor, mais abafado)
typedef struct InstrumentDef { float attack, release, gain; bool oneShot; float darken; } InstrumentDef;

static const InstrumentDef defs[INST_COUNT] = {
    [INST_HORN]          = { 0.010f, 0.30f, 0.9f, false, 0.0f  },
    [INST_TRUMPET]       = { 0.005f, 0.20f, 0.8f, false, 0.0f  },
    [INST_TROMBONE]      = { 0.010f, 0.30f, 0.9f, false, 0.0f  },
    [INST_TUBA]          = { 0.010f, 0.30f, 1.0f, false, 0.0f  },
    [INST_VIOLINS]       = { 0.040f, 0.40f, 0.8f, false, 0.0f  },
    [INST_VIOLINS_SHORT] = { 0.002f, 0.10f, 0.8f, true,  0.0f  },
    [INST_CELLOS]        = { 0.040f, 0.40f, 0.9f, false, 0.0f  },
    [INST_CELLOS_SHORT]  = { 0.002f, 0.10f, 1.0f, true,  0.0f  },
    [INST_CONTRABASS]    = { 0.030f, 0.40f, 1.0f, false, 0.0f  },
    [INST_HARP]          = { 0.002f, 0.30f, 0.8f, true,  0.0f  },
    [INST_FLUTE]         = { 0.020f, 0.25f, 0.8f, false, 0.0f  },
    [INST_OBOE]          = { 0.020f, 0.25f, 0.8f, false, 0.0f  },
    [INST_OCARINA]       = { 0.020f, 0.30f, 0.8f, false, 0.0f  },
    [INST_GLOCK]         = { 0.002f, 0.30f, 0.6f, true,  0.0f  },
    [INST_TIMPANI]       = { 0.002f, 0.30f, 1.0f, true,  0.0f  },
    [INST_SNARE]         = { 0.002f, 0.10f, 0.6f, true,  0.30f },   // caixa bem mais escura
    [INST_BASSDRUM]      = { 0.002f, 0.20f, 1.0f, true,  0.0f  },
    [INST_CYMBAL]        = { 0.002f, 0.40f, 0.6f, true,  0.45f },
    [INST_WARDRUM]       = { 0.002f, 0.20f, 1.0f, true,  0.0f  },
    [INST_GONG]          = { 0.002f, 0.60f, 0.8f, true,  0.0f  },
    [INST_CHOIR]         = { 0.300f, 0.60f, 0.5f, false, 0.0f  },
};

// Equalizador final: mais grave e menos agudo estridente (o som "quente" das trilhas do N64)
typedef struct Biquad { float b0, b1, b2, a1, a2, z1, z2; } Biquad;
static Biquad lowShelf[2], highShelf[2];

static AudioStream stream;
static BankInstrument bank[BANK_COUNT];
static bool bankLoaded = false;
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

static float Noise(void)
{
    noiseState ^= noiseState << 13;
    noiseState ^= noiseState >> 17;
    noiseState ^= noiseState << 5;
    return (float)(noiseState & 0xFFFF)/32768.0f - 1.0f;
}

// ---------------------------------------------------------------------------
// Banco de instrumentos gravados
// ---------------------------------------------------------------------------
static unsigned int ReadU32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }

bool Synth_LoadBank(const unsigned char *data, int size)
{
    if (size < 12 || memcmp(data, "KGSB", 4) != 0) return false;

    const unsigned char *p = data + 12;
    const unsigned char *end = data + size;
    int count = (int)ReadU32(data + 8);
    if (count > BANK_COUNT) count = BANK_COUNT;

    for (int i = 0; i < count && p + 2 <= end; i++)
    {
        BankInstrument *inst = &bank[i];
        inst->unpitched = p[0];
        inst->count = p[1];
        if (inst->count > MAX_BANK_SAMPLES) inst->count = MAX_BANK_SAMPLES;
        p += 2;

        for (int s = 0; s < inst->count && p + 8 <= end; s++)
        {
            float root;
            memcpy(&root, p, 4);
            int frames = (int)ReadU32(p + 4);
            p += 8;
            inst->samples[s] = (BankSample){ root, frames, (const short *)p };
            p += frames*2;
        }
    }
    bankLoaded = true;
    return true;
}

// ---------------------------------------------------------------------------
// MML: c d e f g a b (+ ou # sustenido, - bemol), r pausa, número = duração
// (4 = semínima, 8 = colcheia...), "." pontua, o4 oitava, < > desce/sobe oitava,
// l8 duração padrão, v12 força (0 a 15), [ ... ]3 repete 3 vezes.
// ---------------------------------------------------------------------------
typedef struct MmlState { int octave, length, tick, transpose; float velocity; Channel *ch; } MmlState;

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
                st->ch->events[st->ch->count++] = (NoteEvent){ st->tick, ticks, (st->octave + 1)*12 + semi + st->transpose, st->velocity };
            st->tick += ticks;
        }
        else if (c == '(')
        {
            // Acorde: as notas entre parênteses começam juntas, com a duração depois do ")".
            // Mudanças de oitava dentro do acorde valem só para ele.
            int savedOctave = st->octave;
            int midis[8], n = 0;
            while (s < end && *s && *s != ')')
            {
                char k = *s++;
                if (k >= 'a' && k <= 'g' && n < 8)
                {
                    int semi = semis[k - 'a'];
                    if (*s == '+' || *s == '#') { semi++; s++; }
                    else if (*s == '-') { semi--; s++; }
                    midis[n++] = (st->octave + 1)*12 + semi + st->transpose;
                }
                else if (k == 'o') st->octave = ReadNumber(&s);
                else if (k == '<') st->octave--;
                else if (k == '>') st->octave++;
            }
            if (s < end && *s == ')') s++;
            st->octave = savedOctave;

            int ticks = ReadLength(&s, st->length);
            for (int k = 0; k < n && st->ch->count < MAX_EVENTS; k++)
                st->ch->events[st->ch->count++] = (NoteEvent){ st->tick, ticks, midis[k], st->velocity };
            st->tick += ticks;
        }
        else if (c == 'r') st->tick += ReadLength(&s, st->length);
        else if (c == 'o') st->octave = ReadNumber(&s);
        else if (c == 'k')
        {
            // Transposição em semitons para as notas seguintes: k12 sobe uma oitava, k-12 desce, k0 volta
            int sign = 1;
            if (*s == '-') { sign = -1; s++; }
            int n = ReadNumber(&s);
            st->transpose = sign*((n < 0) ? 0 : n);
        }
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
// Vozes
// ---------------------------------------------------------------------------
static void NoteOn(int channel, const NoteEvent *e)
{
    Channel *ch = &channels[channel];
    const BankSample *sample = NULL;
    double step = 1.0;

    if (ch->instrument < BANK_COUNT)
    {
        // Escolhe a gravação mais próxima da nota e muda a altura dela, como no N64
        const BankInstrument *inst = &bank[ch->instrument];
        if (inst->count == 0) return;
        sample = &inst->samples[0];
        for (int i = 1; i < inst->count; i++)
            if (fabsf(inst->samples[i].root - e->midi) < fabsf(sample->root - e->midi)) sample = &inst->samples[i];
        step = inst->unpitched ? pow(2.0, (e->midi - 60)/12.0) : pow(2.0, (e->midi - sample->root)/12.0);
    }

    // Usa uma voz livre; se não houver, rouba a mais antiga
    Voice *v = NULL;
    for (int i = 0; i < MAX_VOICES && !v; i++) if (!voices[i].active) v = &voices[i];
    if (!v)
    {
        v = &voices[0];
        for (int i = 1; i < MAX_VOICES; i++) if (voices[i].time > v->time) v = &voices[i];
    }

    memset(v, 0, sizeof(*v));
    v->active = true;
    v->instrument = ch->instrument;
    v->channel = channel;
    v->sample = sample;
    v->step = step;
    v->freq = 440.0f*powf(2.0f, (e->midi - 69)/12.0f);
    v->velocity = e->velocity*ch->volume;
    v->holdTime = e->length*60.0f/(bpm*TICKS_PER_BEAT);
    v->released = -1.0f;
    if (defs[v->instrument].oneShot && sample) v->holdTime = (float)(sample->frames/step)/SYNTH_RATE;
}

static float Envelope(Voice *v)
{
    const InstrumentDef *d = &defs[v->instrument];
    float level = (v->time < d->attack) ? v->time/d->attack : 1.0f;
    if (v->released < 0.0f) return level;
    return v->releaseLevel*fmaxf(0.0f, 1.0f - v->released/d->release);
}

// Coral sintetizado: vozes desafinadas com vibrato passando por filtros que formam o "ah"
static float RenderChoir(Voice *v)
{
    static const float formants[3] = { 800.0f, 1150.0f, 2900.0f };
    static const float weights[3] = { 1.0f, 0.6f, 0.25f };
    static const float detune[4] = { 0.993f, 0.998f, 1.003f, 1.008f };

    float vib = 1.0f + 0.006f*sinf(2.0f*PI*5.2f*v->time);
    float src = 0.0f;
    for (int k = 0; k < 4; k++)
    {
        v->phase[k] += v->freq*vib*detune[k]/SYNTH_RATE;
        if (v->phase[k] >= 1.0f) v->phase[k] -= 1.0f;
        src += 2.0f*v->phase[k] - 1.0f;
    }
    src = src*0.25f + Noise()*0.05f;

    // Coeficientes dos filtros calculados uma vez só (isto roda para cada amostra de som)
    static float coef[3] = { 0 };
    if (coef[0] == 0.0f) for (int k = 0; k < 3; k++) coef[k] = 2.0f*sinf(PI*formants[k]/SYNTH_RATE);

    float out = 0.0f;
    for (int k = 0; k < 3; k++)
    {
        float f = coef[k];
        float high = src - v->lowA[k] - 0.25f*v->bandA[k];
        v->bandA[k] += f*high;
        v->lowA[k] += f*v->bandA[k];
        out += v->bandA[k]*weights[k];
    }
    return out*0.6f;
}

static float RenderVoice(Voice *v)
{
    float out;
    if (v->sample)
    {
        int i = (int)v->pos;
        if (i + 1 >= v->sample->frames) { v->active = false; return 0.0f; }
        float frac = (float)(v->pos - i);
        out = (v->sample->data[i]*(1.0f - frac) + v->sample->data[i + 1]*frac)/32768.0f;
        v->pos += v->step;

        float darken = defs[v->instrument].darken;
        if (darken > 0.0f) { v->dark += (out - v->dark)*darken; out = v->dark*1.4f; }
    }
    else if (v->instrument == INST_CHOIR) out = RenderChoir(v);
    else return 0.0f;

    return out*Envelope(v)*defs[v->instrument].gain*v->velocity;
}

// ---------------------------------------------------------------------------
// Reverberação de salão (Freeverb simplificado, estéreo): a "catedral" das trilhas épicas
// ---------------------------------------------------------------------------
#define COMBS 4
static const int combLen[2][COMBS] = { { 810, 862, 927, 984 }, { 827, 879, 944, 1001 } };
static const int allLen[2][2] = { { 403, 320 }, { 420, 337 } };
static float combBuf[2][COMBS][1001], combStore[2][COMBS];
static int combPos[2][COMBS];
static float allBuf[2][2][420];
static int allPos[2][2];

static float Reverb(int side, float in)
{
    float out = 0.0f;
    for (int c = 0; c < COMBS; c++)
    {
        float y = combBuf[side][c][combPos[side][c]];
        combStore[side][c] = y*0.7f + combStore[side][c]*0.3f;   // abafa os agudos do eco
        combBuf[side][c][combPos[side][c]] = in + combStore[side][c]*0.84f;
        combPos[side][c] = (combPos[side][c] + 1) % combLen[side][c];
        out += y;
    }
    for (int a = 0; a < 2; a++)
    {
        float buf = allBuf[side][a][allPos[side][a]];
        float y = -out + buf;
        allBuf[side][a][allPos[side][a]] = out + buf*0.5f;
        allPos[side][a] = (allPos[side][a] + 1) % allLen[side][a];
        out = y;
    }
    return out*0.25f;
}

// Filtros "shelf" (receitas de Robert Bristow-Johnson): realçam ou atenuam graves/agudos
static Biquad MakeShelf(bool low, float freq, float gainDb)
{
    float A = powf(10.0f, gainDb/40.0f);
    float w0 = 2.0f*PI*freq/SYNTH_RATE;
    float cw = cosf(w0), alpha = sinf(w0)/2.0f*sqrtf(2.0f);
    float sa = 2.0f*sqrtf(A)*alpha;
    float b0, b1, b2, a0, a1, a2;
    if (low)
    {
        b0 = A*((A + 1) - (A - 1)*cw + sa);  b1 = 2*A*((A - 1) - (A + 1)*cw);  b2 = A*((A + 1) - (A - 1)*cw - sa);
        a0 = (A + 1) + (A - 1)*cw + sa;      a1 = -2*((A - 1) + (A + 1)*cw);   a2 = (A + 1) + (A - 1)*cw - sa;
    }
    else
    {
        b0 = A*((A + 1) + (A - 1)*cw + sa);  b1 = -2*A*((A - 1) + (A + 1)*cw); b2 = A*((A + 1) + (A - 1)*cw - sa);
        a0 = (A + 1) - (A - 1)*cw + sa;      a1 = 2*((A - 1) - (A + 1)*cw);    a2 = (A + 1) - (A - 1)*cw - sa;
    }
    return (Biquad){ b0/a0, b1/a0, b2/a0, a1/a0, a2/a0, 0.0f, 0.0f };
}

static float ProcessBiquad(Biquad *f, float x)
{
    float y = f->b0*x + f->z1;
    f->z1 = f->b1*x - f->a1*y + f->z2;
    f->z2 = f->b2*x - f->a2*y;
    return y;
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

// Roda na thread de áudio: gera cada amostra na hora, em estéreo
static void SynthCallback(void *buffer, unsigned int frames)
{
    short *out = (short *)buffer;
    double samplesPerTick = SYNTH_RATE*60.0/(bpm*TICKS_PER_BEAT);
    float volume = muted ? 0.0f : masterVolume;

    for (unsigned int i = 0; i < frames; i++)
    {
        float left = 0.0f, right = 0.0f;
        if (playing)
        {
            tickAcc += 1.0;
            while (tickAcc >= samplesPerTick) { tickAcc -= samplesPerTick; StepTick(); }

            for (int l = 0; l < 2; l++) layerGain[l] += (layerTarget[l] - layerGain[l])*0.0002f;

            for (int k = 0; k < MAX_VOICES; k++)
            {
                Voice *v = &voices[k];
                if (!v->active) continue;

                Channel *ch = &channels[v->channel];
                float s = RenderVoice(v)*layerGain[ch->layer];
                left += s*ch->panL;
                right += s*ch->panR;

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

            float dryL = left, dryR = right;
            left = dryL + Reverb(0, (dryL + dryR)*0.5f);
            right = dryR + Reverb(1, (dryL + dryR)*0.5f);

            left = ProcessBiquad(&highShelf[0], ProcessBiquad(&lowShelf[0], left));
            right = ProcessBiquad(&highShelf[1], ProcessBiquad(&lowShelf[1], right));
        }
        out[i*2] = (short)(tanhf(left*volume*0.5f)*30000.0f);    // limita sem estourar
        out[i*2 + 1] = (short)(tanhf(right*volume*0.5f)*30000.0f);
    }
}

void Synth_Init(void)
{
    for (int s = 0; s < 2; s++)
    {
        lowShelf[s] = MakeShelf(true, 140.0f, 6.0f);     // +6 dB nos graves
        highShelf[s] = MakeShelf(false, 5000.0f, -4.0f); // -4 dB nos agudos
    }

    SetAudioStreamBufferSizeDefault(BUFFER_FRAMES);
    stream = LoadAudioStream(SYNTH_RATE, 16, 2);
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
        ch->panL = cosf((sc->pan + 1.0f)*PI/4.0f);   // panorama estéreo com potência constante
        ch->panR = sinf((sc->pan + 1.0f)*PI/4.0f);
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

bool Synth_BankLoaded(void)
{
    return bankLoaded;
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
