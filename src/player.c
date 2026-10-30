#include "game.h"
#include "rlgl.h"

#define RUN_SPEED   6.0f   // velocidade máxima (unidades por segundo)
#define ACCEL      30.0f   // aceleração ao começar a andar
#define DECEL      40.0f   // freio ao soltar o direcional
#define TURN_SPEED 12.0f   // rapidez para virar na direção do movimento

#define ATTACK_TIME   0.22f   // duração de cada golpe (segundos)
#define ATTACK_LUNGE  3.0f    // passo à frente no início do golpe
#define CANCEL_POINT  0.35f   // a partir de que ponto do golpe (0 a 1) um novo aperto já corta para o próximo
#define SHIELD_TURN   10.0f   // rapidez para virar com o escudo levantado

typedef struct AttackInfo {
    float reach;      // alcance a partir do centro do jogador
    float halfArc;    // metade do ângulo coberto pelo golpe (radianos)
    float hitStart;   // trecho do golpe (0 a 1) em que a lâmina machuca
    float hitEnd;
} AttackInfo;

// Combo de 3 golpes: corte para a esquerda, corte de volta, estocada
static const AttackInfo attacks[3] = {
    { 1.7f, 70.0f*DEG2RAD, 0.10f, 0.65f },
    { 1.7f, 70.0f*DEG2RAD, 0.10f, 0.65f },
    { 2.0f, 25.0f*DEG2RAD, 0.15f, 0.70f },
};

void Player_Init(Player *p)
{
    *p = (Player){ 0 };
    p->pos = (Vector3){ 0.0f, 0.0f, 6.0f };
    p->angle = PI;   // começa olhando para os pilares
    p->radius = 0.35f;
}

// Direção desejada no mundo a partir do direcional (relativo à câmera)
static Vector3 WishDirection(float camYaw)
{
    Vector2 move = Input_Move();
    Vector3 forward = { -sinf(camYaw), 0.0f, -cosf(camYaw) };
    Vector3 right = { cosf(camYaw), 0.0f, -sinf(camYaw) };
    return Vector3Add(Vector3Scale(right, move.x), Vector3Scale(forward, -move.y));
}

static void TurnTowards(Player *p, float targetAngle, float speed, float dt)
{
    p->angle = WrapAngle(p->angle + WrapAngle(targetAngle - p->angle)*fminf(1.0f, speed*dt));
}

static void StartAttack(Player *p, int step, Vector3 wishDir)
{
    // Ao iniciar o golpe, vira na hora para onde o direcional aponta
    if (Vector3Length(wishDir) > 0.1f) p->angle = atan2f(wishDir.x, wishDir.z);

    p->state = PSTATE_ATTACK;
    p->attackStep = step;
    p->attackTimer = 0.0f;
    p->attackId++;
}

static void UpdateNormal(Player *p, Vector3 wishDir, float dt)
{
    // Aceleração e freio curtos dão "peso" ao personagem
    Vector3 targetVel = Vector3Scale(wishDir, RUN_SPEED);
    float rate = (Vector3Length(wishDir) > 0.0f) ? ACCEL : DECEL;
    p->vel = Vector3MoveTowards(p->vel, targetVel, rate*dt);

    // Vira suavemente para a direção em que está andando
    if (Vector3Length(p->vel) > 0.1f) TurnTowards(p, atan2f(p->vel.x, p->vel.z), TURN_SPEED, dt);
}

static void UpdateAttack(Player *p, Vector3 wishDir, bool attackPressed, bool shieldHeld, float dt)
{
    const AttackInfo *a = &attacks[p->attackStep];
    float t = p->attackTimer/ATTACK_TIME;

    // Resposta imediata: um novo aperto corta o golpe atual e já começa o próximo do combo
    if (attackPressed && t > CANCEL_POINT)
    {
        StartAttack(p, (p->attackStep + 1)%3, wishDir);
        return;
    }

    // O escudo também interrompe o golpe na hora
    if (shieldHeld && t > CANCEL_POINT)
    {
        p->state = PSTATE_SHIELD;
        return;
    }

    p->attackTimer += dt;
    t = p->attackTimer/ATTACK_TIME;

    // Pequeno avanço no começo do golpe, depois para firme
    Vector3 facing = { sinf(p->angle), 0.0f, cosf(p->angle) };
    if (t < 0.4f) p->vel = Vector3Scale(facing, ATTACK_LUNGE);
    else p->vel = Vector3MoveTowards(p->vel, Vector3Zero(), DECEL*dt);

    if (t >= a->hitStart && t <= a->hitEnd) Dummies_Hit(p->pos, p->angle, a->reach, a->halfArc, p->attackId);

    if (t >= 1.0f) p->state = PSTATE_NORMAL;
}

static void UpdateShield(Player *p, Vector3 wishDir, float dt)
{
    // Com o escudo levantado o jogador fica parado, mas pode virar
    p->vel = Vector3MoveTowards(p->vel, Vector3Zero(), DECEL*dt);
    if (Vector3Length(wishDir) > 0.1f) TurnTowards(p, atan2f(wishDir.x, wishDir.z), SHIELD_TURN, dt);
}

void Player_Update(Player *p, float camYaw, float dt)
{
    Vector3 wishDir = WishDirection(camYaw);
    bool attackPressed = Input_Attack();
    bool shieldHeld = Input_Shield();

    switch (p->state)
    {
        case PSTATE_NORMAL:
            if (attackPressed) StartAttack(p, 0, wishDir);
            else if (shieldHeld) p->state = PSTATE_SHIELD;
            else UpdateNormal(p, wishDir, dt);
            break;

        case PSTATE_ATTACK:
            UpdateAttack(p, wishDir, attackPressed, shieldHeld, dt);
            break;

        case PSTATE_SHIELD:
            if (attackPressed) StartAttack(p, 0, wishDir);
            else if (!shieldHeld) p->state = PSTATE_NORMAL;
            else UpdateShield(p, wishDir, dt);
            break;
    }

    p->pos = Vector3Add(p->pos, Vector3Scale(p->vel, dt));
    p->pos = World_Collide(p->pos, p->radius);
    p->pos = Dummies_Collide(p->pos, p->radius);
}

// Ângulo da espada (graus, em volta do ombro) durante o golpe. Positivo = lado esquerdo.
static float SwingAngle(const Player *p, float t)
{
    float e = 1.0f - (1.0f - t)*(1.0f - t);   // começa rápido e desacelera no fim
    if (p->attackStep == 0) return Lerp(-80.0f, 80.0f, e);
    if (p->attackStep == 1) return Lerp(80.0f, -80.0f, e);
    return 0.0f;
}

static void DrawSwordSwing(const Player *p)
{
    float t = fminf(p->attackTimer/ATTACK_TIME, 1.0f);
    float angle = SwingAngle(p, t);
    float thrust = (p->attackStep == 2) ? sinf(t*PI)*0.6f : 0.0f;   // a estocada vai e volta

    // Rastro da lâmina: um leque claro entre o início do golpe e a posição atual
    if (p->attackStep < 2 && t < 0.8f)
    {
        float start = SwingAngle(p, 0.0f);
        int steps = 6;
        for (int i = 0; i < steps; i++)
        {
            float a0 = Lerp(start, angle, (float)i/steps)*DEG2RAD;
            float a1 = Lerp(start, angle, (float)(i + 1)/steps)*DEG2RAD;
            Vector3 o = { 0.0f, 0.9f, 0.0f };
            Vector3 v0 = { sinf(a0)*1.5f, 0.9f, cosf(a0)*1.5f };
            Vector3 v1 = { sinf(a1)*1.5f, 0.9f, cosf(a1)*1.5f };
            DrawTriangle3D(o, v0, v1, Fade(WHITE, 0.35f));
            DrawTriangle3D(o, v1, v0, Fade(WHITE, 0.35f));
        }
    }

    rlPushMatrix();
        rlTranslatef(0.0f, 0.9f, 0.0f);
        rlRotatef(angle, 0.0f, 1.0f, 0.0f);
        DrawCube((Vector3){ 0.0f, 0.0f, 0.75f + thrust }, 0.06f, 0.06f, 1.0f, WHITE);   // lâmina
        DrawCube((Vector3){ 0.0f, 0.0f, 0.25f + thrust }, 0.25f, 0.06f, 0.06f, GOLD);   // guarda
    rlPopMatrix();
}

// Mateo provisório, desenhado já virado para a direção em que olha
void Player_Draw(const Player *p)
{
    // Sombra redonda no chão, como no N64
    DrawCylinder((Vector3){ p->pos.x, 0.01f, p->pos.z }, 0.45f, 0.45f, 0.01f, 16, Fade(BLACK, 0.3f));

    rlPushMatrix();
        rlTranslatef(p->pos.x, p->pos.y, p->pos.z);
        rlRotatef(p->angle*RAD2DEG, 0.0f, 1.0f, 0.0f);
        DrawCube((Vector3){ 0.0f, 0.6f, 0.0f }, 0.6f, 1.2f, 0.4f, (Color){ 40, 70, 160, 255 });   // corpo
        DrawSphere((Vector3){ 0.0f, 1.45f, 0.0f }, 0.3f, (Color){ 240, 200, 90, 255 });         // cabeça loira

        // Escudo: na frente quando levantado, no braço esquerdo no resto do tempo
        if (p->state == PSTATE_SHIELD) DrawCube((Vector3){ 0.1f, 0.8f, 0.45f }, 0.6f, 0.7f, 0.1f, LIGHTGRAY);
        else DrawCube((Vector3){ 0.4f, 0.7f, 0.1f }, 0.1f, 0.6f, 0.5f, LIGHTGRAY);

        // Espada: animada durante o golpe, apontando para a frente no braço direito no resto do tempo
        if (p->state == PSTATE_ATTACK) DrawSwordSwing(p);
        else DrawCube((Vector3){ -0.4f, 0.7f, 0.5f }, 0.06f, 0.06f, 0.9f, WHITE);
    rlPopMatrix();
}
