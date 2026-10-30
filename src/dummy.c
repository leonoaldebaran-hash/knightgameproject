#include "game.h"
#include "rlgl.h"

#define MAX_DUMMIES    3
#define DUMMY_HP       3      // golpes até cair
#define DUMMY_RADIUS   0.35f
#define DUMMY_RESPAWN  3.0f   // segundos até levantar de novo
#define WOBBLE_KICK    250.0f // força do balanço ao levar um golpe
#define WOBBLE_SPRING  80.0f
#define WOBBLE_DAMP    5.0f

typedef struct Dummy {
    Vector3 pos;
    int hp;
    float flash;       // tempo restante do brilho de "acertou"
    float wobble;      // inclinação atual (graus)
    float wobbleVel;
    float tiltDir;     // direção para onde o boneco tomba (radianos)
    float downTimer;   // tempo caído
    int lastHitId;     // último golpe que acertou, para não contar duas vezes
} Dummy;

static Dummy dummies[MAX_DUMMIES];
static int totalHits = 0;

void Dummies_Init(void)
{
    const Vector3 spots[MAX_DUMMIES] = {
        { -2.0f, 0.0f, -3.0f },
        {  1.0f, 0.0f, -5.0f },
        { -5.0f, 0.0f, -6.0f },
    };
    for (int i = 0; i < MAX_DUMMIES; i++) dummies[i] = (Dummy){ .pos = spots[i], .hp = DUMMY_HP, .lastHitId = -1 };
    totalHits = 0;
}

void Dummies_Update(float dt)
{
    for (int i = 0; i < MAX_DUMMIES; i++)
    {
        Dummy *d = &dummies[i];

        // Balanço de mola depois do golpe
        d->wobbleVel -= d->wobble*WOBBLE_SPRING*dt;
        d->wobbleVel *= fmaxf(0.0f, 1.0f - WOBBLE_DAMP*dt);
        d->wobble += d->wobbleVel*dt;

        if (d->flash > 0.0f) d->flash -= dt;

        if (d->hp <= 0)
        {
            d->downTimer -= dt;
            if (d->downTimer <= 0.0f) { d->hp = DUMMY_HP; d->wobble = 0.0f; d->wobbleVel = 0.0f; }
        }
    }
}

// Verifica quais bonecos estão dentro do arco do golpe. Retorna quantos foram acertados.
int Dummies_Hit(Vector3 origin, float angle, float reach, float halfArc, int attackId)
{
    int hits = 0;
    for (int i = 0; i < MAX_DUMMIES; i++)
    {
        Dummy *d = &dummies[i];
        if (d->hp <= 0 || d->lastHitId == attackId) continue;

        float dx = d->pos.x - origin.x, dz = d->pos.z - origin.z;
        float dist = sqrtf(dx*dx + dz*dz);
        if (dist > reach + DUMMY_RADIUS) continue;

        // Muito perto conta sempre; senão precisa estar dentro do ângulo do golpe
        float dirAngle = atan2f(dx, dz);
        if (dist > DUMMY_RADIUS + 0.3f && fabsf(WrapAngle(dirAngle - angle)) > halfArc) continue;

        d->lastHitId = attackId;
        d->hp--;
        d->flash = 0.15f;
        d->tiltDir = dirAngle;
        d->wobbleVel += WOBBLE_KICK;
        if (d->hp <= 0) d->downTimer = DUMMY_RESPAWN;
        hits++;
    }
    totalHits += hits;
    return hits;
}

// Empurra o jogador para fora dos bonecos em pé
Vector3 Dummies_Collide(Vector3 pos, float radius)
{
    for (int i = 0; i < MAX_DUMMIES; i++)
    {
        Dummy *d = &dummies[i];
        if (d->hp <= 0) continue;

        float dx = pos.x - d->pos.x, dz = pos.z - d->pos.z;
        float minDist = radius + DUMMY_RADIUS;
        float dist2 = dx*dx + dz*dz;
        if (dist2 >= minDist*minDist || dist2 < 0.0001f) continue;

        float dist = sqrtf(dist2);
        pos.x = d->pos.x + dx/dist*minDist;
        pos.z = d->pos.z + dz/dist*minDist;
    }
    return pos;
}

int Dummies_Count(void)
{
    return MAX_DUMMIES;
}

// Posição do boneco para a mira travada; falso se ele estiver caído
bool Dummies_TargetPos(int i, Vector3 *pos)
{
    if (dummies[i].hp <= 0) return false;
    *pos = dummies[i].pos;
    return true;
}

int Dummies_TotalHits(void)
{
    return totalHits;
}

// Boneco de treino de palha com uma placa de armadura
void Dummies_Draw(void)
{
    const Color straw = { 200, 170, 100, 255 };

    for (int i = 0; i < MAX_DUMMIES; i++)
    {
        Dummy *d = &dummies[i];
        Color body = (d->flash > 0.0f) ? WHITE : straw;
        Color armor = (d->flash > 0.0f) ? WHITE : (Color){ 150, 150, 165, 255 };
        float tilt = (d->hp <= 0) ? 85.0f : d->wobble;   // caído ou balançando

        DrawCylinder((Vector3){ d->pos.x, 0.01f, d->pos.z }, 0.4f, 0.4f, 0.01f, 12, Fade(BLACK, 0.3f));

        rlPushMatrix();
            rlTranslatef(d->pos.x, 0.0f, d->pos.z);
            rlRotatef(d->tiltDir*RAD2DEG, 0.0f, 1.0f, 0.0f);
            rlRotatef(tilt, 1.0f, 0.0f, 0.0f);
            DrawCylinder((Vector3){ 0.0f, 0.0f, 0.0f }, 0.06f, 0.06f, 0.6f, 6, BROWN);   // estaca
            DrawCube((Vector3){ 0.0f, 0.95f, 0.0f }, 0.55f, 0.7f, 0.3f, body);           // tronco de palha
            DrawCube((Vector3){ 0.0f, 1.0f, 0.0f }, 0.6f, 0.45f, 0.36f, armor);          // placa de armadura
            DrawCube((Vector3){ 0.0f, 1.0f, 0.0f }, 1.1f, 0.1f, 0.1f, body);             // braços
            DrawSphere((Vector3){ 0.0f, 1.5f, 0.0f }, 0.22f, body);                      // cabeça
        rlPopMatrix();
    }
}
