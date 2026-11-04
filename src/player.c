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
#define SHIELD_ARC    (75.0f*DEG2RAD)   // golpes vindos até esse ângulo da frente são defendidos

#define START_HP      6       // 3 corações
#define HURT_TIME     0.35f   // tempo sem controle depois de levar um golpe
#define HURT_PUSH     7.0f    // força do empurrão ao levar um golpe
#define INVULN_TIME   1.0f    // tempo piscando sem levar dano
#define BLOCK_PUSH    3.0f    // empurrão ao defender com o escudo

#define STRAFE_SPEED  4.5f    // andando em volta do alvo com a mira travada
#define LOCK_TURN     15.0f   // rapidez para virar de frente para o alvo
#define SIDEHOP_TIME  0.35f
#define SIDEHOP_SPEED 9.0f
#define SIDEHOP_HIGH  0.5f
#define BACKFLIP_TIME 0.45f
#define BACKFLIP_SPEED 7.5f
#define BACKFLIP_HIGH 1.0f

#define ROLL_TIME     0.45f   // duração do rolamento
#define ROLL_SPEED    9.0f
#define ROLL_SAFE     0.30f   // começo do rolamento em que o jogador não leva dano

#define GRAVITY       25.0f   // queda (unidades por segundo²)
#define JUMP_VEL      6.5f    // impulso do pulo automático (~0,85 de altura)
#define AUTOJUMP_SPEED 4.5f   // acima dessa velocidade, sair de uma borda vira pulo
#define STEP_DOWN     0.3f    // descer até isso continua "pisando" (degraus)
#define AIR_CONTROL   0.35f   // quanto dá para corrigir a direção no ar

#define CHARGE_TIME   0.7f   // segurando o ataque esse tempo, o golpe giratório fica pronto
#define CHARGE_SPEED  1.8f    // dá para andar devagar enquanto carrega
#define SPIN_TIME     0.40f   // duração do golpe giratório
#define SPIN_REACH    2.3f    // alcance em volta do jogador

static const Vector3 spawnPos = { 0.0f, 0.0f, 6.0f };

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
    p->pos = spawnPos;
    p->angle = PI;   // começa olhando para os pilares
    p->radius = 0.35f;
    p->hp = p->maxHp = START_HP;
    p->grounded = true;
}

HitResult Player_TakeHit(Player *p, Vector3 from, int damage)
{
    // Durante a esquiva o jogador não leva dano: é o momento de escapar do golpe
    if (p->invulnTimer > 0.0f || p->state == PSTATE_HURT || p->state == PSTATE_DODGE) return HIT_IGNORED;
    if (p->state == PSTATE_ROLL && p->rollTimer < ROLL_SAFE) return HIT_IGNORED;

    float dx = from.x - p->pos.x, dz = from.z - p->pos.z;
    float toAttacker = atan2f(dx, dz);
    Vector3 away = Vector3Normalize((Vector3){ -dx, 0.0f, -dz });

    // Escudo levantado e virado para o atacante: defende
    if (p->state == PSTATE_SHIELD && fabsf(WrapAngle(toAttacker - p->angle)) < SHIELD_ARC)
    {
        p->vel = Vector3Scale(away, BLOCK_PUSH);
        p->blockFlash = 0.12f;
        Sfx_Play(SFX_BLOCK);
        return HIT_BLOCKED;
    }

    Sfx_Play(SFX_HURT);
    Sfx_Play(SFX_VOICE_HURT);

    p->hp -= damage;
    p->state = PSTATE_HURT;
    p->hurtTimer = 0.0f;
    p->vel = Vector3Scale(away, HURT_PUSH);
    p->invulnTimer = INVULN_TIME;
    return HIT_TAKEN;
}

static void UpdateHurt(Player *p, float dt)
{
    p->hurtTimer += dt;
    p->vel = Vector3MoveTowards(p->vel, Vector3Zero(), DECEL*0.5f*dt);
    if (p->hurtTimer < HURT_TIME) return;

    if (p->hp <= 0)
    {
        // Por enquanto, cair só leva de volta ao início com a vida cheia
        p->pos = spawnPos;
        p->vel = Vector3Zero();
        p->velY = 0.0f;
        p->grounded = true;
        p->angle = PI;
        p->hp = p->maxHp;
        p->fallenTimer = 2.5f;
    }
    p->state = PSTATE_NORMAL;
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

static float AngleToTarget(const Player *p)
{
    Vector3 t = Target_Pos();
    return atan2f(t.x - p->pos.x, t.z - p->pos.z);
}

static void StartAttack(Player *p, int step, Vector3 wishDir)
{
    // Ao iniciar o golpe, vira na hora para o alvo travado ou para onde o direcional aponta
    if (Target_Active()) p->angle = AngleToTarget(p);
    else if (Vector3Length(wishDir) > 0.1f) p->angle = atan2f(wishDir.x, wishDir.z);

    p->state = PSTATE_ATTACK;
    p->attackStep = step;
    p->attackTimer = 0.0f;
    p->attackId++;
    Sfx_Play(SFX_SWING);
    if (step != 1) Sfx_Play(SFX_VOICE_ATTACK);   // grita no 1º e no 3º golpe do combo
}

// Com a mira travada: anda de lado em volta do alvo, sempre de frente para ele
static void UpdateLocked(Player *p, Vector3 wishDir, float dt)
{
    Vector3 targetVel = Vector3Scale(wishDir, STRAFE_SPEED);
    float rate = (Vector3Length(wishDir) > 0.0f) ? ACCEL : DECEL;
    p->vel = Vector3MoveTowards(p->vel, targetVel, rate*dt);
    TurnTowards(p, AngleToTarget(p), LOCK_TURN, dt);
}

// Salto lateral (direcional para o lado) ou mortal para trás (direcional para trás)
static bool TryDodge(Player *p, Vector3 wishDir)
{
    if (Vector3Length(wishDir) < 0.3f) return false;

    float a = AngleToTarget(p);
    Vector3 toTarget = { sinf(a), 0.0f, cosf(a) };
    Vector3 left = { cosf(a), 0.0f, -sinf(a) };
    float forward = Vector3DotProduct(wishDir, toTarget);
    float side = Vector3DotProduct(wishDir, left);

    if (!p->grounded) return false;

    if (forward < -0.5f)
    {
        p->dodgeBack = true;
        p->vel = Vector3Scale(toTarget, -BACKFLIP_SPEED);
        p->velY = sqrtf(2.0f*GRAVITY*BACKFLIP_HIGH);
    }
    else if (fabsf(side) > 0.3f)
    {
        p->dodgeBack = false;
        p->dodgeSide = (side > 0.0f) ? -1.0f : 1.0f;
        p->vel = Vector3Scale(left, (side > 0.0f) ? SIDEHOP_SPEED : -SIDEHOP_SPEED);
        p->velY = sqrtf(2.0f*GRAVITY*SIDEHOP_HIGH);
    }
    else return false;   // para a frente: no futuro, o golpe com pulo

    p->grounded = false;
    p->angle = a;
    p->state = PSTATE_DODGE;
    p->dodgeTimer = 0.0f;
    Sfx_Play(SFX_SWING);
    return true;
}

static void UpdateDodge(Player *p, float dt)
{
    // O pulo em si é feito pela gravidade (UpdateVertical); aqui só termina ao pousar
    p->dodgeTimer += dt;
    if (Target_Active()) TurnTowards(p, AngleToTarget(p), LOCK_TURN, dt);

    if (p->grounded && p->dodgeTimer > 0.1f)
    {
        p->vel = Vector3Scale(p->vel, 0.3f);   // aterrissa freando
        p->state = PSTATE_NORMAL;
    }
}

static void UpdateNormal(Player *p, Vector3 wishDir, float dt)
{
    // Aceleração e freio curtos dão "peso" ao personagem
    Vector3 targetVel = Vector3Scale(wishDir, RUN_SPEED);
    float rate = (Vector3Length(wishDir) > 0.0f) ? ACCEL : DECEL;
    if (!p->grounded) rate *= AIR_CONTROL;   // no ar dá para corrigir só um pouco
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

    if (t >= a->hitStart && t <= a->hitEnd)
    {
        int hits = Dummies_Hit(p->pos, p->angle, a->reach, a->halfArc, p->attackId)
                 + Soldiers_Hit(p->pos, p->angle, a->reach, a->halfArc, p->attackId);
        if (hits > 0) Sfx_Play(SFX_HIT);
    }

    // Golpe terminou com o botão ainda segurado: começa a carregar o golpe giratório
    if (t >= 1.0f)
    {
        p->state = Input_AttackHeld() ? PSTATE_CHARGE : PSTATE_NORMAL;
        p->chargeTime = 0.0f;
    }
}

// Rolamento: corre e rola para onde o direcional aponta
static void StartRoll(Player *p, Vector3 wishDir)
{
    p->angle = atan2f(wishDir.x, wishDir.z);
    p->state = PSTATE_ROLL;
    p->rollTimer = 0.0f;
    Sfx_Play(SFX_SWING);
}

static void UpdateRoll(Player *p, float dt)
{
    p->rollTimer += dt;
    float t = fminf(p->rollTimer/ROLL_TIME, 1.0f);
    Vector3 facing = { sinf(p->angle), 0.0f, cosf(p->angle) };
    p->vel = Vector3Scale(facing, ROLL_SPEED*(1.0f - 0.4f*t));
    if (t >= 1.0f)
    {
        p->vel = Vector3Scale(p->vel, 0.5f);
        p->state = PSTATE_NORMAL;
    }
}

// Carregando: anda devagar com a espada para trás; soltar com a carga cheia gira
static void UpdateCharge(Player *p, Vector3 wishDir, bool shieldHeld, float dt)
{
    bool wasReady = p->chargeTime >= CHARGE_TIME;
    p->chargeTime += dt;
    if (!wasReady && p->chargeTime >= CHARGE_TIME) Sfx_Play(SFX_ALERT);   // "tim": está pronto

    p->vel = Vector3MoveTowards(p->vel, Vector3Scale(wishDir, CHARGE_SPEED), ACCEL*dt);
    if (Target_Active()) TurnTowards(p, AngleToTarget(p), LOCK_TURN, dt);

    if (shieldHeld) p->state = PSTATE_SHIELD;
    else if (!Input_AttackHeld())
    {
        if (p->chargeTime >= CHARGE_TIME)
        {
            p->state = PSTATE_SPIN;
            p->spinTimer = 0.0f;
            p->attackId++;
            Sfx_Play(SFX_SWING);
            Sfx_Play(SFX_VOICE_ATTACK);
        }
        else p->state = PSTATE_NORMAL;
    }
}

// Golpe giratório: acerta tudo em volta
static void UpdateSpin(Player *p, float dt)
{
    p->spinTimer += dt;
    float t = p->spinTimer/SPIN_TIME;
    p->vel = Vector3MoveTowards(p->vel, Vector3Zero(), DECEL*dt);

    if (t >= 0.1f && t <= 0.9f)
    {
        int hits = Dummies_Hit(p->pos, p->angle, SPIN_REACH, PI, p->attackId)
                 + Soldiers_Hit(p->pos, p->angle, SPIN_REACH, PI, p->attackId);
        if (hits > 0) Sfx_Play(SFX_HIT);
    }
    if (t >= 1.0f) p->state = PSTATE_NORMAL;
}

static void UpdateShield(Player *p, Vector3 wishDir, float dt)
{
    // Com o escudo levantado o jogador fica parado, mas pode virar
    p->vel = Vector3MoveTowards(p->vel, Vector3Zero(), DECEL*dt);
    if (Target_Active()) TurnTowards(p, AngleToTarget(p), LOCK_TURN, dt);
    else if (Vector3Length(wishDir) > 0.1f) TurnTowards(p, atan2f(wishDir.x, wishDir.z), SHIELD_TURN, dt);
}

// Altura: sobe degraus baixos sozinho, cai das bordas e pula sozinho quando sai correndo
static void UpdateVertical(Player *p, float dt)
{
    float ground = World_GroundHeight(p->pos);
    p->groundY = ground;

    if (p->grounded)
    {
        if (ground >= p->pos.y - STEP_DOWN)
        {
            p->pos.y = ground;
            p->velY = 0.0f;
            return;
        }

        // Saiu de uma borda: correndo vira pulo (como no Ocarina), andando só cai
        p->grounded = false;
        float speed = sqrtf(p->vel.x*p->vel.x + p->vel.z*p->vel.z);
        bool canJump = (p->state == PSTATE_NORMAL || p->state == PSTATE_ROLL);
        if (canJump && speed > AUTOJUMP_SPEED)
        {
            p->velY = JUMP_VEL;
            Sfx_Play(SFX_VOICE_ATTACK);
        }
        else p->velY = 0.0f;
    }

    p->velY -= GRAVITY*dt;
    p->pos.y += p->velY*dt;
    if (p->pos.y <= ground && p->velY <= 0.0f)
    {
        p->pos.y = ground;
        p->velY = 0.0f;
        p->grounded = true;
    }
}

void Player_Update(Player *p, float camYaw, float dt)
{
    Vector3 wishDir = WishDirection(camYaw);
    bool attackPressed = Input_Attack();
    bool shieldHeld = Input_Shield();
    bool dodgePressed = Input_Dodge();

    if (p->invulnTimer > 0.0f) p->invulnTimer -= dt;
    if (p->blockFlash > 0.0f) p->blockFlash -= dt;
    if (p->fallenTimer > 0.0f) p->fallenTimer -= dt;

    switch (p->state)
    {
        case PSTATE_NORMAL:
            if (attackPressed) StartAttack(p, 0, wishDir);
            else if (shieldHeld) p->state = PSTATE_SHIELD;
            else if (Target_Active() && dodgePressed && TryDodge(p, wishDir)) break;
            else if (!Target_Active() && dodgePressed && Vector3Length(wishDir) > 0.3f) StartRoll(p, wishDir);
            else if (Target_Active()) UpdateLocked(p, wishDir, dt);
            else UpdateNormal(p, wishDir, dt);
            break;

        case PSTATE_DODGE:
            UpdateDodge(p, dt);
            break;

        case PSTATE_ROLL:
            UpdateRoll(p, dt);
            break;

        case PSTATE_CHARGE:
            UpdateCharge(p, wishDir, shieldHeld, dt);
            break;

        case PSTATE_SPIN:
            UpdateSpin(p, dt);
            break;

        case PSTATE_ATTACK:
            UpdateAttack(p, wishDir, attackPressed, shieldHeld, dt);
            break;

        case PSTATE_SHIELD:
            if (attackPressed) StartAttack(p, 0, wishDir);
            else if (!shieldHeld) p->state = PSTATE_NORMAL;
            else UpdateShield(p, wishDir, dt);
            break;

        case PSTATE_HURT:
            UpdateHurt(p, dt);
            break;
    }

    p->pos = Vector3Add(p->pos, Vector3Scale(p->vel, dt));
    p->pos = World_Collide(p->pos, p->radius);
    p->pos = Dummies_Collide(p->pos, p->radius);
    p->pos = Soldiers_Collide(p->pos, p->radius);
    UpdateVertical(p, dt);
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
    // Sombra no chão embaixo dele, menor quanto mais alto ele estiver (ajuda a mirar o pulo)
    float shadow = 0.45f*Clamp(1.0f - (p->pos.y - p->groundY)*0.25f, 0.4f, 1.0f);
    DrawCylinder((Vector3){ p->pos.x, p->groundY + 0.01f, p->pos.z }, shadow, shadow, 0.01f, 16, Fade(BLACK, 0.3f));

    // Pisca enquanto está invulnerável depois de levar dano
    if (p->invulnTimer > 0.0f && fmodf(p->invulnTimer, 0.12f) < 0.06f) return;

    Color body = (p->state == PSTATE_HURT) ? (Color){ 200, 60, 60, 255 } : (Color){ 40, 70, 160, 255 };
    Color shield = (p->blockFlash > 0.0f) ? YELLOW : LIGHTGRAY;

    rlPushMatrix();
        rlTranslatef(p->pos.x, p->pos.y, p->pos.z);
        rlRotatef(p->angle*RAD2DEG, 0.0f, 1.0f, 0.0f);

        // Mortal para trás gira o corpo inteiro; o salto lateral só inclina para o lado
        if (p->state == PSTATE_DODGE)
        {
            float t = fminf(p->dodgeTimer/(p->dodgeBack ? BACKFLIP_TIME : SIDEHOP_TIME), 1.0f);
            rlTranslatef(0.0f, 0.7f, 0.0f);
            if (p->dodgeBack) rlRotatef(-360.0f*t, 1.0f, 0.0f, 0.0f);
            else rlRotatef(p->dodgeSide*15.0f, 0.0f, 0.0f, 1.0f);
            rlTranslatef(0.0f, -0.7f, 0.0f);
        }

        // Rolamento: cambalhota para a frente, bem rente ao chão
        if (p->state == PSTATE_ROLL)
        {
            float t = fminf(p->rollTimer/ROLL_TIME, 1.0f);
            rlTranslatef(0.0f, 0.45f, 0.0f);
            rlRotatef(360.0f*t, 1.0f, 0.0f, 0.0f);
            rlTranslatef(0.0f, -0.45f, 0.0f);
        }

        // Golpe giratório: o corpo inteiro dá uma volta, com um anel de rastro da lâmina
        if (p->state == PSTATE_SPIN)
        {
            float t = fminf(p->spinTimer/SPIN_TIME, 1.0f);
            float turn = 360.0f*(1.0f - (1.0f - t)*(1.0f - t));
            for (int i = 0; i < 16; i++)
            {
                float a0 = (float)i/16.0f*turn*DEG2RAD, a1 = (float)(i + 1)/16.0f*turn*DEG2RAD;
                Vector3 o = { 0.0f, 0.8f, 0.0f };
                Vector3 v0 = { -cosf(a0)*SPIN_REACH, 0.8f, sinf(a0)*SPIN_REACH };
                Vector3 v1 = { -cosf(a1)*SPIN_REACH, 0.8f, sinf(a1)*SPIN_REACH };
                Color trail = Fade((Color){ 255, 240, 160, 255 }, 0.4f*(1.0f - t));
                DrawTriangle3D(o, v0, v1, trail);
                DrawTriangle3D(o, v1, v0, trail);
            }
            rlRotatef(turn, 0.0f, 1.0f, 0.0f);
        }

        DrawCube((Vector3){ 0.0f, 0.6f, 0.0f }, 0.6f, 1.2f, 0.4f, body);                         // corpo
        DrawSphere((Vector3){ 0.0f, 1.45f, 0.0f }, 0.3f, (Color){ 240, 200, 90, 255 });         // cabeça loira

        // Escudo: na frente quando levantado, no braço esquerdo no resto do tempo
        if (p->state == PSTATE_SHIELD) DrawCube((Vector3){ 0.1f, 0.8f, 0.45f }, 0.6f, 0.7f, 0.1f, shield);
        else DrawCube((Vector3){ 0.4f, 0.7f, 0.1f }, 0.1f, 0.6f, 0.5f, shield);

        // Espada: animada durante o golpe; para trás e brilhando enquanto carrega; esticada para
        // o lado no giro; apontando para a frente no braço direito no resto do tempo
        if (p->state == PSTATE_ATTACK) DrawSwordSwing(p);
        else if (p->state == PSTATE_CHARGE)
        {
            bool ready = p->chargeTime >= CHARGE_TIME;
            Color blade = ready ? ((fmodf(p->chargeTime, 0.16f) < 0.08f) ? YELLOW : WHITE)
                                : ColorLerp(WHITE, (Color){ 120, 200, 255, 255 }, p->chargeTime/CHARGE_TIME);
            rlPushMatrix();
                rlTranslatef(-0.35f, 0.9f, 0.0f);
                rlRotatef(-140.0f, 0.0f, 1.0f, 0.0f);
                DrawCube((Vector3){ 0.0f, 0.0f, 0.6f }, 0.06f, 0.06f, 1.0f, blade);
                if (ready) DrawSphere((Vector3){ 0.0f, 0.0f, 1.1f }, 0.12f, Fade(YELLOW, 0.7f));
            rlPopMatrix();
        }
        else if (p->state == PSTATE_SPIN)
        {
            rlPushMatrix();
                rlTranslatef(-0.35f, 0.8f, 0.0f);
                rlRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
                DrawCube((Vector3){ 0.0f, 0.0f, 0.9f }, 0.06f, 0.06f, 1.3f, (Color){ 255, 240, 160, 255 });
            rlPopMatrix();
        }
        else DrawCube((Vector3){ -0.4f, 0.7f, 0.5f }, 0.06f, 0.06f, 0.9f, WHITE);
    rlPopMatrix();
}
