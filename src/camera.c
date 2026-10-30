#include "game.h"

#define CAM_DISTANCE    7.0f
#define CAM_HEIGHT      3.5f
#define CAM_ORBIT_SPEED 2.5f   // radianos por segundo
#define CAM_FOLLOW      6.0f   // suavidade ao seguir o jogador
#define CAM_RECENTER    8.0f   // rapidez para voltar para trás do jogador

void GameCamera_Init(GameCamera *c)
{
    *c = (GameCamera){ 0 };
    c->cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    c->cam.fovy = 45.0f;
    c->cam.projection = CAMERA_PERSPECTIVE;
}

void GameCamera_Update(GameCamera *c, const Player *p, float dt)
{
    c->yaw -= Input_Orbit()*CAM_ORBIT_SPEED*dt;
    if (Input_Recenter()) c->yaw += WrapAngle(p->angle + PI - c->yaw)*fminf(1.0f, CAM_RECENTER*dt);
    c->yaw = WrapAngle(c->yaw);

    // O foco segue o jogador com um pequeno atraso
    Vector3 focusTarget = { p->pos.x, p->pos.y + 1.0f, p->pos.z };
    c->focus = Vector3Lerp(c->focus, focusTarget, fminf(1.0f, CAM_FOLLOW*dt));

    c->cam.target = c->focus;
    c->cam.position = (Vector3){ c->focus.x + sinf(c->yaw)*CAM_DISTANCE,
                                 c->focus.y + CAM_HEIGHT,
                                 c->focus.z + cosf(c->yaw)*CAM_DISTANCE };
}
