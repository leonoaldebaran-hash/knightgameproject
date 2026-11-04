#include "game.h"
#include "rlgl.h"

// Boneco de treino: ator (EnDummy) imóvel, que pode ser mirado mas não é hostil.
// Leva golpes por um colisor AC; balança, cai e se levanta sozinho.

#define DUMMY_HP       3      // golpes até cair
#define DUMMY_RADIUS   0.35f
#define DUMMY_HEIGHT   1.7f
#define DUMMY_RESPAWN  3.0f   // segundos até levantar de novo
#define DUMMY_COOLDOWN 0.25f  // depois de um golpe, ignora a mesma lâmina por um instante
#define WOBBLE_KICK    250.0f // força do balanço ao levar um golpe
#define WOBBLE_SPRING  80.0f
#define WOBBLE_DAMP    5.0f

static const unsigned char damageTable[DMG_COUNT] = { [DMG_SLASH] = 1, [DMG_SPIN] = 2, [DMG_JUMP] = 2 };
static int totalHits = 0;

typedef struct EnDummy {
    Actor actor;
    Collider bodyAC, bodyOC;
    float flash;       // tempo restante do brilho de "acertou"
    float wobble;      // inclinação atual (graus)
    float wobbleVel;
    float tiltDir;     // direção para onde o boneco tomba (radianos)
    float downTimer;   // tempo caído
    float cooldown;
} EnDummy;

static void Init(Actor *a)
{
    EnDummy *d = (EnDummy *)a;
    a->colChk.health = DUMMY_HP;
    a->colChk.damageTable = damageTable;
    a->colChk.mass = MASS_IMMOVABLE;
    a->targetHeight = 2.0f;

    Collider_InitCylinder(&d->bodyAC, a, DUMMY_RADIUS, DUMMY_HEIGHT);
    d->bodyAC.acFlags = AC_ON | AC_TYPE_PLAYER;
    d->bodyAC.bumpDmg = DMG_BIT(DMG_SLASH) | DMG_BIT(DMG_SPIN) | DMG_BIT(DMG_JUMP);
    Collider_InitCylinder(&d->bodyOC, a, DUMMY_RADIUS, DUMMY_HEIGHT);
    d->bodyOC.ocFlags = OC_ON;
}

static void Update(Actor *a, float dt)
{
    EnDummy *d = (EnDummy *)a;

    if ((d->bodyAC.acFlags & AC_HIT) && a->colChk.health > 0)
    {
        Actor_ApplyDamage(a, d->bodyAC.acHitDmg, d->bodyAC.acHitDamage);
        Vector3 from = d->bodyAC.acHit ? d->bodyAC.acHit->pos : a->pos;
        d->tiltDir = atan2f(a->pos.x - from.x, a->pos.z - from.z);
        d->flash = 0.15f;
        d->wobbleVel += WOBBLE_KICK;
        d->cooldown = DUMMY_COOLDOWN;
        totalHits++;
        if (a->colChk.health <= 0)
        {
            d->downTimer = DUMMY_RESPAWN;
            a->flags &= ~ACTOR_FLAG_TARGETABLE;
        }
    }
    d->bodyAC.acFlags &= ~AC_HIT;

    // Balanço de mola depois do golpe
    d->wobbleVel -= d->wobble*WOBBLE_SPRING*dt;
    d->wobbleVel *= fmaxf(0.0f, 1.0f - WOBBLE_DAMP*dt);
    d->wobble += d->wobbleVel*dt;

    if (d->flash > 0.0f) d->flash -= dt;
    if (d->cooldown > 0.0f) d->cooldown -= dt;

    if (a->colChk.health <= 0)
    {
        d->downTimer -= dt;
        if (d->downTimer <= 0.0f)
        {
            a->colChk.health = DUMMY_HP;
            a->flags |= ACTOR_FLAG_TARGETABLE;
            d->wobble = d->wobbleVel = 0.0f;
        }
        return;
    }

    d->bodyAC.pos = d->bodyOC.pos = a->pos;
    if (d->cooldown <= 0.0f) CollisionCheck_SetAC(&d->bodyAC);
    CollisionCheck_SetOC(&d->bodyOC);
}

// Boneco de treino de palha com uma placa de armadura
static void Draw(Actor *a)
{
    EnDummy *d = (EnDummy *)a;
    const Color straw = { 200, 170, 100, 255 };
    Color body = (d->flash > 0.0f) ? WHITE : straw;
    Color armor = (d->flash > 0.0f) ? WHITE : (Color){ 150, 150, 165, 255 };
    float tilt = (a->colChk.health <= 0) ? 85.0f : d->wobble;   // caído ou balançando

    DrawCylinder((Vector3){ a->pos.x, 0.01f, a->pos.z }, 0.4f, 0.4f, 0.01f, 12, Fade(BLACK, 0.3f));

    rlPushMatrix();
        rlTranslatef(a->pos.x, 0.0f, a->pos.z);
        rlRotatef(d->tiltDir*RAD2DEG, 0.0f, 1.0f, 0.0f);
        rlRotatef(tilt, 1.0f, 0.0f, 0.0f);
        DrawCylinder((Vector3){ 0.0f, 0.0f, 0.0f }, 0.06f, 0.06f, 0.6f, 6, BROWN);   // estaca
        DrawCube((Vector3){ 0.0f, 0.95f, 0.0f }, 0.55f, 0.7f, 0.3f, body);           // tronco de palha
        DrawCube((Vector3){ 0.0f, 1.0f, 0.0f }, 0.6f, 0.45f, 0.36f, armor);          // placa de armadura
        DrawCube((Vector3){ 0.0f, 1.0f, 0.0f }, 1.1f, 0.1f, 0.1f, body);             // braços
        DrawSphere((Vector3){ 0.0f, 1.5f, 0.0f }, 0.22f, body);                      // cabeça
    rlPopMatrix();
}

int Dummies_TotalHits(void)
{
    return totalHits;
}

const ActorProfile EnDummy_Profile = {
    .name = "EnDummy",
    .category = ACTORCAT_PROP,
    .flags = ACTOR_FLAG_TARGETABLE,
    .size = sizeof(EnDummy),
    .init = Init,
    .update = Update,
    .draw = Draw,
};
