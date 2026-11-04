#include "game.h"
#include "rlgl.h"

// Soldado de armadura: ator inimigo (EnSoldier) no modelo do Ocarina.
// params escolhe a rota de patrulha. O corpo leva dano por um colisor AC com tabela de dano;
// a espada é um colisor AT que varre o espaço só durante o golpe; o corpo empurra (OC).

#define SOLDIER_HP     4      // golpes de espada até cair
#define SOLDIER_RADIUS 0.4f
#define SOLDIER_HEIGHT 1.9f
#define SOLDIER_DAMAGE 1      // meio coração

#define SIGHT_RANGE    9.0f   // distância em que nota o jogador
#define GIVEUP_RANGE   14.0f  // distância em que desiste de perseguir
#define PATROL_SPEED   0.9f
#define CHASE_SPEED    2.3f   // mais lento que a corrida do jogador (~3,1 m/s), para dar para fugir
#define TURN_SPEED     6.0f
#define ATTACK_RANGE   1.5f   // distância em que para e começa o golpe

#define WINDUP_TIME    0.55f  // espada levantada: o aviso de que o golpe vem
#define STRIKE_TIME    0.2f
#define RECOVER_TIME   0.7f   // brecha para o jogador contra-atacar
#define STAGGER_TIME   1.1f   // brecha maior quando o golpe bate no escudo
#define HURT_TIME      0.35f
#define RESPAWN_TIME   5.0f

// Tabela de dano: corte 1, giratório 2, golpe com pulo 2 (como os inimigos do Ocarina)
static const unsigned char damageTable[DMG_COUNT] = { [DMG_SLASH] = 1, [DMG_SPIN] = 2, [DMG_JUMP] = 2 };

static const Vector3 routes[][2] = {
    { { -6.0f, 0.0f, -9.0f }, { 6.0f, 0.0f, -9.0f } },
    { {  8.0f, 0.0f,  4.0f }, { 8.0f, 0.0f, -3.0f } },
};

typedef enum {
    SOLDIER_PATROL, SOLDIER_CHASE, SOLDIER_WINDUP, SOLDIER_STRIKE,
    SOLDIER_RECOVER, SOLDIER_HURT, SOLDIER_DEAD,
} SoldierState;

typedef struct EnSoldier {
    Actor actor;
    Collider bodyAC, bodyOC, swordAT;
    SoldierState state;
    float timer;            // tempo no estado atual
    Vector3 patrolA, patrolB;
    bool toB;               // patrulhando em direção ao ponto B
    float flash;            // brilho de "levou golpe"
} EnSoldier;

static void SetState(EnSoldier *s, SoldierState state)
{
    s->state = state;
    s->timer = 0.0f;
}

static void Reset(EnSoldier *s)
{
    s->actor.pos = s->patrolA;
    s->actor.vel = Vector3Zero();
    s->actor.colChk.health = SOLDIER_HP;
    s->actor.flags = EnSoldier_Profile.flags;
    s->toB = true;
    s->flash = 0.0f;
    SetState(s, SOLDIER_PATROL);
}

static void Init(Actor *a)
{
    EnSoldier *s = (EnSoldier *)a;
    int route = a->params % (int)(sizeof(routes)/sizeof(routes[0]));
    s->patrolA = routes[route][0];
    s->patrolB = routes[route][1];
    a->colChk.damageTable = damageTable;
    a->colChk.mass = 80;   // armadura pesada: empurra o jogador mais do que é empurrado
    a->targetHeight = 2.3f;

    Collider_InitCylinder(&s->bodyAC, a, SOLDIER_RADIUS, SOLDIER_HEIGHT);
    s->bodyAC.acFlags = AC_ON | AC_TYPE_PLAYER;
    s->bodyAC.bumpDmg = DMG_BIT(DMG_SLASH) | DMG_BIT(DMG_SPIN) | DMG_BIT(DMG_JUMP);
    Collider_InitCylinder(&s->bodyOC, a, SOLDIER_RADIUS, SOLDIER_HEIGHT);
    s->bodyOC.ocFlags = OC_ON;
    Collider_InitQuad(&s->swordAT, a);
    s->swordAT.atFlags = AT_ON | AT_TYPE_ENEMY;
    s->swordAT.touchDmg = DMG_BIT(DMG_ENEMY_SWORD);
    s->swordAT.damage = SOLDIER_DAMAGE;
    Reset(s);
}

static void TurnTowards(EnSoldier *s, float targetAngle, float speed, float dt)
{
    s->actor.yaw = WrapAngle(s->actor.yaw + WrapAngle(targetAngle - s->actor.yaw)*fminf(1.0f, speed*dt));
}

// Inclinação da espada (graus em volta do ombro): positivo aponta para baixo, negativo para cima
static float SwordPitch(const EnSoldier *s)
{
    float t;
    switch (s->state)
    {
        case SOLDIER_WINDUP:
            t = fminf(s->timer/WINDUP_TIME, 1.0f);
            return Lerp(30.0f, -110.0f, 1.0f - (1.0f - t)*(1.0f - t));
        case SOLDIER_STRIKE:
            t = fminf(s->timer/STRIKE_TIME, 1.0f);
            return Lerp(-110.0f, 30.0f, t);
        default:
            return 30.0f;
    }
}

// Lâmina no mundo: do ombro direito, girando em volta do eixo lateral
static void SwordSegment(const EnSoldier *s, Vector3 *base, Vector3 *tip)
{
    float pitch = SwordPitch(s)*DEG2RAD, yaw = s->actor.yaw;
    Vector3 fwd = { sinf(yaw), 0.0f, cosf(yaw) }, left = { cosf(yaw), 0.0f, -sinf(yaw) };
    Vector3 shoulder = Vector3Add(s->actor.pos, Vector3Add(Vector3Scale(left, -0.42f), (Vector3){ 0.0f, 1.15f, 0.0f }));
    shoulder = Vector3Add(shoulder, Vector3Scale(fwd, 0.1f));
    Vector3 dir = Vector3Add(Vector3Scale(fwd, cosf(pitch)), (Vector3){ 0.0f, -sinf(pitch), 0.0f });
    *base = Vector3Add(shoulder, Vector3Scale(dir, 0.1f));
    *tip = Vector3Add(shoulder, Vector3Scale(dir, 1.15f));
}

// Lê o resultado das colisões do quadro anterior
static void ReactToCollisions(EnSoldier *s)
{
    Vector3 facing = { sinf(s->actor.yaw), 0.0f, cosf(s->actor.yaw) };

    if (s->swordAT.atFlags & AT_BOUNCED)
    {
        // Bateu no escudo: recua e fica aberto por mais tempo
        SetState(s, SOLDIER_RECOVER);
        s->timer = RECOVER_TIME - STAGGER_TIME;
        s->actor.vel = Vector3Scale(facing, -4.0f);
    }

    if ((s->bodyAC.acFlags & AC_HIT) && s->state != SOLDIER_HURT && s->state != SOLDIER_DEAD)
    {
        // Levar um golpe interrompe o que ele estava fazendo, inclusive o ataque
        Actor_ApplyDamage(&s->actor, s->bodyAC.acHitDmg, s->bodyAC.acHitDamage);
        Vector3 from = s->bodyAC.acHit ? s->bodyAC.acHit->pos : s->actor.pos;
        Vector3 away = Vector3Subtract(s->actor.pos, from);
        away.y = 0.0f;
        s->actor.vel = Vector3Scale(Vector3Normalize(away), 5.0f);
        s->flash = 0.15f;
        if (s->actor.colChk.health <= 0)
        {
            SetState(s, SOLDIER_DEAD);
            s->actor.flags &= ~(ACTOR_FLAG_TARGETABLE | ACTOR_FLAG_HOSTILE);
        }
        else SetState(s, SOLDIER_HURT);
    }

    s->swordAT.atFlags &= ~(AT_HIT | AT_BOUNCED);
    s->bodyAC.acFlags &= ~AC_HIT;
}

static void Update(Actor *a, float dt)
{
    EnSoldier *s = (EnSoldier *)a;
    float dist = a->xzDistToPlayer;
    float toPlayer = a->yawTowardsPlayer;
    Vector3 facing = { sinf(a->yaw), 0.0f, cosf(a->yaw) };

    ReactToCollisions(s);
    s->timer += dt;
    if (s->flash > 0.0f) s->flash -= dt;

    switch (s->state)
    {
        case SOLDIER_PATROL:
        {
            Vector3 target = s->toB ? s->patrolB : s->patrolA;
            Vector3 toTarget = Vector3Subtract(target, a->pos);
            toTarget.y = 0.0f;
            if (Vector3Length(toTarget) < 0.3f) s->toB = !s->toB;
            else
            {
                a->vel = Vector3Scale(Vector3Normalize(toTarget), PATROL_SPEED);
                TurnTowards(s, atan2f(toTarget.x, toTarget.z), TURN_SPEED, dt);
            }
            if (dist < SIGHT_RANGE) SetState(s, SOLDIER_CHASE);
        } break;

        case SOLDIER_CHASE:
            TurnTowards(s, toPlayer, TURN_SPEED, dt);
            if (dist > GIVEUP_RANGE) SetState(s, SOLDIER_PATROL);
            else if (dist > ATTACK_RANGE) a->vel = Vector3Scale((Vector3){ sinf(toPlayer), 0.0f, cosf(toPlayer) }, CHASE_SPEED);
            else
            {
                a->vel = Vector3Zero();
                if (fabsf(WrapAngle(toPlayer - a->yaw)) < 25.0f*DEG2RAD)
                {
                    SetState(s, SOLDIER_WINDUP);
                    Sfx_Play(SFX_ALERT);
                }
            }
            break;

        case SOLDIER_WINDUP:
            // Vira devagar enquanto prepara o golpe: dá para circular em volta dele
            a->vel = Vector3Zero();
            TurnTowards(s, toPlayer, TURN_SPEED*0.3f, dt);
            if (s->timer >= WINDUP_TIME)
            {
                SetState(s, SOLDIER_STRIKE);
                s->swordAT.quadFresh = true;
                Sfx_Play(SFX_SWING);
                Sfx_Play(SFX_VOICE_ENEMY);
            }
            break;

        case SOLDIER_STRIKE:
            a->vel = Vector3Scale(facing, 2.0f);
            if (s->timer >= STRIKE_TIME) SetState(s, SOLDIER_RECOVER);
            break;

        case SOLDIER_RECOVER:
        case SOLDIER_HURT:
            a->vel = Vector3MoveTowards(a->vel, Vector3Zero(), 12.0f*dt);
            if (s->state == SOLDIER_RECOVER && s->timer >= RECOVER_TIME) SetState(s, SOLDIER_CHASE);
            if (s->state == SOLDIER_HURT && s->timer >= HURT_TIME) SetState(s, SOLDIER_CHASE);
            break;

        case SOLDIER_DEAD:
            a->vel = Vector3Zero();
            if (s->timer >= RESPAWN_TIME) Reset(s);
            return;
    }

    a->pos = Vector3Add(a->pos, Vector3Scale(a->vel, dt));
    a->pos = World_Collide(a->pos, SOLDIER_RADIUS);

    // Registra os colisores deste quadro
    s->bodyAC.pos = s->bodyOC.pos = a->pos;
    if (s->state != SOLDIER_HURT) CollisionCheck_SetAC(&s->bodyAC);
    CollisionCheck_SetOC(&s->bodyOC);
    if (s->state == SOLDIER_STRIKE)
    {
        Vector3 base, tip;
        SwordSegment(s, &base, &tip);
        Collider_SetQuad(&s->swordAT, base, tip);
        CollisionCheck_SetAT(&s->swordAT);
    }
}

// Soldado de armadura carmesim com detalhes dourados
static void Draw(Actor *a)
{
    EnSoldier *s = (EnSoldier *)a;
    bool dead = (s->state == SOLDIER_DEAD);
    bool lit = (s->flash > 0.0f);

    Color armor = lit ? WHITE : (Color){ 130, 30, 45, 255 };
    Color trim  = lit ? WHITE : (Color){ 200, 170, 80, 255 };
    Color legs  = lit ? WHITE : (Color){ 60, 55, 65, 255 };
    Color eyes  = (s->state == SOLDIER_WINDUP) ? RED : BLACK;   // olhos acendem antes do golpe

    DrawCylinder((Vector3){ a->pos.x, 0.01f, a->pos.z }, 0.5f, 0.5f, 0.01f, 12, Fade(BLACK, 0.3f));

    rlPushMatrix();
        rlTranslatef(a->pos.x, 0.0f, a->pos.z);
        rlRotatef(a->yaw*RAD2DEG, 0.0f, 1.0f, 0.0f);
        if (dead) rlRotatef(-85.0f, 1.0f, 0.0f, 0.0f);   // caído de costas

        DrawCube((Vector3){  0.15f, 0.35f, 0.0f }, 0.22f, 0.7f, 0.26f, legs);
        DrawCube((Vector3){ -0.15f, 0.35f, 0.0f }, 0.22f, 0.7f, 0.26f, legs);
        DrawCube((Vector3){ 0.0f, 1.0f, 0.0f }, 0.66f, 0.62f, 0.42f, armor);       // peitoral
        DrawCube((Vector3){ 0.0f, 0.72f, 0.0f }, 0.7f, 0.1f, 0.46f, trim);         // cinturão
        DrawCube((Vector3){  0.44f, 1.28f, 0.0f }, 0.3f, 0.16f, 0.44f, trim);      // ombreiras
        DrawCube((Vector3){ -0.44f, 1.28f, 0.0f }, 0.3f, 0.16f, 0.44f, trim);
        DrawSphere((Vector3){ 0.0f, 1.56f, 0.0f }, 0.27f, armor);                  // elmo
        DrawCube((Vector3){ 0.0f, 1.86f, -0.02f }, 0.06f, 0.24f, 0.42f, trim);     // crista do elmo
        DrawCube((Vector3){ 0.0f, 1.56f, 0.24f }, 0.3f, 0.05f, 0.06f, eyes);       // fenda dos olhos

        if (!dead)
        {
            rlPushMatrix();
                rlTranslatef(-0.42f, 1.15f, 0.1f);
                rlRotatef(SwordPitch(s), 1.0f, 0.0f, 0.0f);
                DrawCube((Vector3){ 0.0f, 0.0f, 0.6f }, 0.07f, 0.07f, 1.0f, LIGHTGRAY);   // lâmina
                DrawCube((Vector3){ 0.0f, 0.0f, 0.1f }, 0.25f, 0.07f, 0.07f, trim);       // guarda
            rlPopMatrix();
        }
    rlPopMatrix();
}

const ActorProfile EnSoldier_Profile = {
    .name = "EnSoldier",
    .category = ACTORCAT_ENEMY,
    .flags = ACTOR_FLAG_TARGETABLE | ACTOR_FLAG_HOSTILE,
    .size = sizeof(EnSoldier),
    .init = Init,
    .update = Update,
    .draw = Draw,
};
