#include "game.h"
#include "rlgl.h"

#define MAX_SOLDIERS   2
#define SOLDIER_HP     4      // golpes de espada até cair
#define SOLDIER_RADIUS 0.4f
#define SOLDIER_DAMAGE 1      // meio coração

#define SIGHT_RANGE    9.0f   // distância em que nota o jogador
#define GIVEUP_RANGE   14.0f  // distância em que desiste de perseguir
#define PATROL_SPEED   1.5f
#define CHASE_SPEED    3.2f   // mais lento que o jogador, para dar para fugir
#define TURN_SPEED     6.0f
#define ATTACK_RANGE   1.5f   // distância em que para e começa o golpe

#define WINDUP_TIME    0.55f  // espada levantada: o aviso de que o golpe vem
#define STRIKE_TIME    0.2f
#define RECOVER_TIME   0.7f   // brecha para o jogador contra-atacar
#define STAGGER_TIME   1.1f   // brecha maior quando o golpe bate no escudo
#define HURT_TIME      0.35f
#define RESPAWN_TIME   5.0f

#define STRIKE_REACH   1.8f
#define STRIKE_ARC     (60.0f*DEG2RAD)

typedef enum {
    SOLDIER_PATROL,
    SOLDIER_CHASE,
    SOLDIER_WINDUP,
    SOLDIER_STRIKE,
    SOLDIER_RECOVER,
    SOLDIER_HURT,
    SOLDIER_DEAD,
} SoldierState;

typedef struct Soldier {
    Vector3 pos, vel;
    float angle;
    int hp;
    SoldierState state;
    float timer;            // tempo no estado atual
    Vector3 patrolA, patrolB;
    bool toB;               // patrulhando em direção ao ponto B
    bool struck;            // o golpe atual já foi resolvido
    float flash;            // brilho de "levou golpe"
    int lastHitId;
} Soldier;

static Soldier soldiers[MAX_SOLDIERS];

static void SetState(Soldier *s, SoldierState state)
{
    s->state = state;
    s->timer = 0.0f;
}

static void ResetSoldier(Soldier *s)
{
    s->pos = s->patrolA;
    s->vel = Vector3Zero();
    s->hp = SOLDIER_HP;
    s->toB = true;
    s->flash = 0.0f;
    s->lastHitId = -1;
    SetState(s, SOLDIER_PATROL);
}

void Soldiers_Init(void)
{
    const Vector3 routes[MAX_SOLDIERS][2] = {
        { { -6.0f, 0.0f, -9.0f }, { 6.0f, 0.0f, -9.0f } },
        { {  8.0f, 0.0f,  4.0f }, { 8.0f, 0.0f, -3.0f } },
    };
    for (int i = 0; i < MAX_SOLDIERS; i++)
    {
        soldiers[i] = (Soldier){ .patrolA = routes[i][0], .patrolB = routes[i][1] };
        ResetSoldier(&soldiers[i]);
    }
}

static void TurnTowards(Soldier *s, float targetAngle, float speed, float dt)
{
    s->angle = WrapAngle(s->angle + WrapAngle(targetAngle - s->angle)*fminf(1.0f, speed*dt));
}

static void UpdateSoldier(Soldier *s, Player *p, float dt)
{
    float dx = p->pos.x - s->pos.x, dz = p->pos.z - s->pos.z;
    float dist = sqrtf(dx*dx + dz*dz);
    float toPlayer = atan2f(dx, dz);
    Vector3 facing = { sinf(s->angle), 0.0f, cosf(s->angle) };

    s->timer += dt;
    if (s->flash > 0.0f) s->flash -= dt;

    switch (s->state)
    {
        case SOLDIER_PATROL:
        {
            Vector3 target = s->toB ? s->patrolB : s->patrolA;
            Vector3 toTarget = Vector3Subtract(target, s->pos);
            toTarget.y = 0.0f;
            if (Vector3Length(toTarget) < 0.3f) s->toB = !s->toB;
            else
            {
                s->vel = Vector3Scale(Vector3Normalize(toTarget), PATROL_SPEED);
                TurnTowards(s, atan2f(toTarget.x, toTarget.z), TURN_SPEED, dt);
            }
            if (dist < SIGHT_RANGE) SetState(s, SOLDIER_CHASE);
        } break;

        case SOLDIER_CHASE:
            TurnTowards(s, toPlayer, TURN_SPEED, dt);
            if (dist > GIVEUP_RANGE) SetState(s, SOLDIER_PATROL);
            else if (dist > ATTACK_RANGE) s->vel = Vector3Scale((Vector3){ dx/dist, 0.0f, dz/dist }, CHASE_SPEED);
            else
            {
                s->vel = Vector3Zero();
                if (fabsf(WrapAngle(toPlayer - s->angle)) < 25.0f*DEG2RAD)
                {
                    SetState(s, SOLDIER_WINDUP);
                    Sfx_Play(SFX_ALERT);
                }
            }
            break;

        case SOLDIER_WINDUP:
            // Vira devagar enquanto prepara o golpe: dá para circular em volta dele
            s->vel = Vector3Zero();
            TurnTowards(s, toPlayer, TURN_SPEED*0.3f, dt);
            if (s->timer >= WINDUP_TIME)
            {
                SetState(s, SOLDIER_STRIKE);
                s->struck = false;
                Sfx_Play(SFX_SWING);
                Sfx_Play(SFX_VOICE_ENEMY);
            }
            break;

        case SOLDIER_STRIKE:
            s->vel = Vector3Scale(facing, 2.0f);
            if (!s->struck && dist < STRIKE_REACH + p->radius && fabsf(WrapAngle(toPlayer - s->angle)) < STRIKE_ARC)
            {
                s->struck = true;
                if (Player_TakeHit(p, s->pos, SOLDIER_DAMAGE) == HIT_BLOCKED)
                {
                    // Bateu no escudo: recua e fica aberto por mais tempo
                    SetState(s, SOLDIER_RECOVER);
                    s->timer = RECOVER_TIME - STAGGER_TIME;
                    s->vel = Vector3Scale(facing, -4.0f);
                    break;
                }
            }
            if (s->timer >= STRIKE_TIME) SetState(s, SOLDIER_RECOVER);
            break;

        case SOLDIER_RECOVER:
        case SOLDIER_HURT:
            s->vel = Vector3MoveTowards(s->vel, Vector3Zero(), 12.0f*dt);
            if (s->state == SOLDIER_RECOVER && s->timer >= RECOVER_TIME) SetState(s, SOLDIER_CHASE);
            if (s->state == SOLDIER_HURT && s->timer >= HURT_TIME) SetState(s, SOLDIER_CHASE);
            break;

        case SOLDIER_DEAD:
            s->vel = Vector3Zero();
            if (s->timer >= RESPAWN_TIME) ResetSoldier(s);
            return;
    }

    s->pos = Vector3Add(s->pos, Vector3Scale(s->vel, dt));
    s->pos = World_Collide(s->pos, SOLDIER_RADIUS);

    // Não entra dentro do jogador
    float minDist = SOLDIER_RADIUS + p->radius;
    dx = s->pos.x - p->pos.x; dz = s->pos.z - p->pos.z;
    float d = sqrtf(dx*dx + dz*dz);
    if (d < minDist && d > 0.0001f)
    {
        s->pos.x = p->pos.x + dx/d*minDist;
        s->pos.z = p->pos.z + dz/d*minDist;
    }
}

void Soldiers_Update(Player *p, float dt)
{
    for (int i = 0; i < MAX_SOLDIERS; i++) UpdateSoldier(&soldiers[i], p, dt);
}

int Soldiers_Hit(Vector3 origin, float angle, float reach, float halfArc, int attackId)
{
    int hits = 0;
    for (int i = 0; i < MAX_SOLDIERS; i++)
    {
        Soldier *s = &soldiers[i];
        if (s->state == SOLDIER_DEAD || s->lastHitId == attackId) continue;

        float dx = s->pos.x - origin.x, dz = s->pos.z - origin.z;
        float dist = sqrtf(dx*dx + dz*dz);
        if (dist > reach + SOLDIER_RADIUS) continue;

        float dirAngle = atan2f(dx, dz);
        if (dist > SOLDIER_RADIUS + 0.3f && fabsf(WrapAngle(dirAngle - angle)) > halfArc) continue;

        // Levar um golpe interrompe o que o soldado estava fazendo, inclusive o ataque
        s->lastHitId = attackId;
        s->hp--;
        s->flash = 0.15f;
        s->vel = Vector3Scale((Vector3){ sinf(dirAngle), 0.0f, cosf(dirAngle) }, 5.0f);
        SetState(s, (s->hp <= 0) ? SOLDIER_DEAD : SOLDIER_HURT);
        hits++;
    }
    return hits;
}

int Soldiers_Count(void)
{
    return MAX_SOLDIERS;
}

// Posição do soldado para a mira travada; falso se ele estiver caído
bool Soldiers_TargetPos(int i, Vector3 *pos)
{
    if (soldiers[i].state == SOLDIER_DEAD) return false;
    *pos = soldiers[i].pos;
    return true;
}

// Algum soldado está lutando com o jogador? (usado para trocar a música)
bool Soldiers_Engaged(void)
{
    for (int i = 0; i < MAX_SOLDIERS; i++)
    {
        SoldierState st = soldiers[i].state;
        if (st != SOLDIER_PATROL && st != SOLDIER_DEAD) return true;
    }
    return false;
}

Vector3 Soldiers_Collide(Vector3 pos, float radius)
{
    for (int i = 0; i < MAX_SOLDIERS; i++)
    {
        Soldier *s = &soldiers[i];
        if (s->state == SOLDIER_DEAD) continue;

        float dx = pos.x - s->pos.x, dz = pos.z - s->pos.z;
        float minDist = radius + SOLDIER_RADIUS;
        float dist2 = dx*dx + dz*dz;
        if (dist2 >= minDist*minDist || dist2 < 0.0001f) continue;

        float dist = sqrtf(dist2);
        pos.x = s->pos.x + dx/dist*minDist;
        pos.z = s->pos.z + dz/dist*minDist;
    }
    return pos;
}

// Inclinação da espada (graus em volta do ombro): positivo aponta para baixo, negativo para cima
static float SwordPitch(const Soldier *s)
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

// Soldado de armadura carmesim com detalhes dourados
void Soldiers_Draw(void)
{
    for (int i = 0; i < MAX_SOLDIERS; i++)
    {
        Soldier *s = &soldiers[i];
        bool dead = (s->state == SOLDIER_DEAD);
        bool lit = (s->flash > 0.0f);

        Color armor = lit ? WHITE : (Color){ 130, 30, 45, 255 };
        Color trim  = lit ? WHITE : (Color){ 200, 170, 80, 255 };
        Color legs  = lit ? WHITE : (Color){ 60, 55, 65, 255 };
        Color eyes  = (s->state == SOLDIER_WINDUP) ? RED : BLACK;   // olhos acendem antes do golpe

        DrawCylinder((Vector3){ s->pos.x, 0.01f, s->pos.z }, 0.5f, 0.5f, 0.01f, 12, Fade(BLACK, 0.3f));

        rlPushMatrix();
            rlTranslatef(s->pos.x, 0.0f, s->pos.z);
            rlRotatef(s->angle*RAD2DEG, 0.0f, 1.0f, 0.0f);
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
}
