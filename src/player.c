#include "game.h"
#include "rlgl.h"

#define RUN_SPEED   6.0f   // velocidade máxima (unidades por segundo)
#define ACCEL      30.0f   // aceleração ao começar a andar
#define DECEL      40.0f   // freio ao soltar o direcional
#define TURN_SPEED 12.0f   // rapidez para virar na direção do movimento

void Player_Init(Player *p)
{
    *p = (Player){ 0 };
    p->pos = (Vector3){ 0.0f, 0.0f, 6.0f };
    p->angle = PI;   // começa olhando para os pilares
    p->radius = 0.35f;
}

void Player_Update(Player *p, float camYaw, float dt)
{
    Vector2 move = Input_Move();

    // O direcional é relativo à câmera: "cima" anda para onde a câmera olha
    Vector3 forward = { -sinf(camYaw), 0.0f, -cosf(camYaw) };
    Vector3 right = { cosf(camYaw), 0.0f, -sinf(camYaw) };
    Vector3 wishDir = Vector3Add(Vector3Scale(right, move.x), Vector3Scale(forward, -move.y));

    // Aceleração e freio curtos dão "peso" ao personagem
    Vector3 targetVel = Vector3Scale(wishDir, RUN_SPEED);
    float rate = (Vector2Length(move) > 0.0f) ? ACCEL : DECEL;
    p->vel = Vector3MoveTowards(p->vel, targetVel, rate*dt);

    p->pos = Vector3Add(p->pos, Vector3Scale(p->vel, dt));
    p->pos = World_Collide(p->pos, p->radius);

    // Vira suavemente para a direção em que está andando
    if (Vector3Length(p->vel) > 0.1f)
    {
        float targetAngle = atan2f(p->vel.x, p->vel.z);
        p->angle = WrapAngle(p->angle + WrapAngle(targetAngle - p->angle)*fminf(1.0f, TURN_SPEED*dt));
    }
}

// Mateo provisório, desenhado já virado para a direção em que anda
void Player_Draw(const Player *p)
{
    // Sombra redonda no chão, como no N64
    DrawCylinder((Vector3){ p->pos.x, 0.01f, p->pos.z }, 0.45f, 0.45f, 0.01f, 16, Fade(BLACK, 0.3f));

    rlPushMatrix();
        rlTranslatef(p->pos.x, p->pos.y, p->pos.z);
        rlRotatef(p->angle*RAD2DEG, 0.0f, 1.0f, 0.0f);
        DrawCube((Vector3){ 0.0f, 0.6f, 0.0f }, 0.6f, 1.2f, 0.4f, (Color){ 40, 70, 160, 255 });   // corpo
        DrawSphere((Vector3){ 0.0f, 1.45f, 0.0f }, 0.3f, (Color){ 240, 200, 90, 255 });         // cabeça loira
        DrawCube((Vector3){ 0.4f, 0.7f, 0.1f }, 0.1f, 0.6f, 0.5f, LIGHTGRAY);                     // escudo (braço esquerdo)
        DrawCube((Vector3){ -0.4f, 0.7f, 0.5f }, 0.06f, 0.06f, 0.9f, WHITE);                      // espada (braço direito)
    rlPopMatrix();
}
