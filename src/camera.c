#include "game.h"

// Câmera no modelo do Ocarina (estudado no Ship of Harkinian), com três modos:
//
// NORMAL   Distância elástica: a câmera fica parada no mundo enquanto o jogador está entre
//          CAM_DIST_MIN e CAM_DIST_MAX dela; só acompanha quando ele passa desses limites.
//          Gira devagar para trás do jogador apenas enquanto ele anda (parado, não gira).
//          Olha ~10° para baixo, campo de visão de 60°.
// PARALELO Mira segurada sem alvo: vai rápido para trás do jogador, horizontal, com zoom (45°).
// BATALHA  Mira travada num alvo: mais perto, atrás do jogador e um pouco para o lado,
//          enquadrando o jogador e o alvo, campo de visão de 50°.

// NORMAL0 do Ocarina: distância 200 a 300, inclinação 10°, giro de volta em ~12 quadros, FOV 60
#define CAM_DIST_MIN      OOT_DIST(200.0f)
#define CAM_DIST_MAX      OOT_DIST(300.0f)
#define CAM_PITCH         (10.0f*DEG2RAD)
#define CAM_YAW_TIME      OOT_FRAMES(12.0f)   // tempo para girar para trás andando na velocidade máxima
#define CAM_FOV           60.0f
#define CAM_AT_HEIGHT     1.4f                // foco ~80% da altura do jogador (yOffset -20%)

// PARALLEL do Ocarina: distância 250, horizontal, FOV 45
#define PARA_DIST         OOT_DIST(250.0f)
#define PARA_PITCH        (2.0f*DEG2RAD)
#define PARA_FOV          45.0f
#define PARA_YAW_TIME     OOT_FRAMES(5.0f)

// BATTLE do Ocarina: distância 180, FOV 50, a câmera desloca para o lado para mostrar os dois
#define BATT_DIST         OOT_DIST(180.0f)
#define BATT_PITCH        (12.0f*DEG2RAD)
#define BATT_FOV          50.0f
#define BATT_SWING        (20.0f*DEG2RAD)
#define BATT_AT_MIX       0.35f
#define BATT_YAW_TIME     OOT_FRAMES(6.0f)

#define CAM_ORBIT_SPEED   2.5f    // giro manual (Q/E ou analógico direito), como a câmera livre do Ship of Harkinian
#define CAM_SMOOTH        6.0f    // suavidade das trocas de distância, inclinação e zoom
#define CAM_FOLLOW        10.0f   // suavidade do foco seguindo o jogador

void GameCamera_Init(GameCamera *c)
{
    *c = (GameCamera){ 0 };
    c->cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    c->cam.fovy = CAM_FOV;
    c->cam.projection = CAMERA_PERSPECTIVE;
    c->dist = CAM_DIST_MIN;
    c->pitch = CAM_PITCH;
    c->fov = CAM_FOV;
    c->eye = (Vector3){ 0.0f, 0.0f, 6.0f + CAM_DIST_MIN };
    c->focus = (Vector3){ 0.0f, CAM_AT_HEIGHT, 6.0f };
}

static float Approach(float value, float target, float rate, float dt)
{
    return value + (target - value)*(1.0f - expf(-rate*dt));
}

static float ApproachAngle(float value, float target, float time, float dt)
{
    return WrapAngle(value + WrapAngle(target - value)*(1.0f - expf(-dt/time)));
}

void GameCamera_Update(GameCamera *c, const Player *p, float dt)
{
    // O foco acompanha a altura do chão (escadas, plataformas), mas não cada pulo
    Vector3 focusTarget = { p->pos.x, p->groundY + CAM_AT_HEIGHT, p->pos.z };
    float behind = WrapAngle(p->angle + PI);   // direção atrás do jogador
    float wantDist, wantPitch, wantFov;

    // Direção atual da câmera em volta do foco, a partir de onde ela está no mundo
    float yaw = atan2f(c->eye.x - c->focus.x, c->eye.z - c->focus.z);

    if (Target_Active())
    {
        // BATALHA: atrás do jogador em relação ao alvo, um pouco para o lado
        Vector3 t = Target_Pos();
        float awayFromTarget = atan2f(p->pos.x - t.x, p->pos.z - t.z);
        yaw = ApproachAngle(yaw, awayFromTarget + BATT_SWING, BATT_YAW_TIME, dt);
        focusTarget = Vector3Lerp(focusTarget, (Vector3){ t.x, p->groundY + CAM_AT_HEIGHT, t.z }, BATT_AT_MIX);
        wantDist = BATT_DIST;
        wantPitch = BATT_PITCH;
        wantFov = BATT_FOV;
    }
    else if (Input_Target())
    {
        // PARALELO: mira sem alvo, câmera vai rápido para trás do jogador
        yaw = ApproachAngle(yaw, behind, PARA_YAW_TIME, dt);
        wantDist = PARA_DIST;
        wantPitch = PARA_PITCH;
        wantFov = PARA_FOV;
    }
    else
    {
        // NORMAL: só gira para trás enquanto o jogador anda, mais rápido quanto mais rápido ele vai
        float speed = sqrtf(p->vel.x*p->vel.x + p->vel.z*p->vel.z);
        float moving = Clamp(speed/OOT_SPEED(6.0f), 0.0f, 1.0f);
        if (moving > 0.05f) yaw = WrapAngle(yaw + WrapAngle(behind - yaw)*(1.0f - expf(-dt*moving/CAM_YAW_TIME)));
        yaw = WrapAngle(yaw - Input_Orbit()*CAM_ORBIT_SPEED*dt);

        // Distância elástica: mantém a distância atual dentro dos limites
        float dx = c->eye.x - focusTarget.x, dz = c->eye.z - focusTarget.z;
        wantDist = Clamp(sqrtf(dx*dx + dz*dz), CAM_DIST_MIN, CAM_DIST_MAX);
        wantPitch = CAM_PITCH;
        wantFov = CAM_FOV;
    }

    c->focus = Vector3Lerp(c->focus, focusTarget, 1.0f - expf(-CAM_FOLLOW*dt));
    c->dist = Target_Active() || Input_Target() ? Approach(c->dist, wantDist, CAM_SMOOTH, dt) : wantDist;
    c->pitch = Approach(c->pitch, wantPitch, CAM_SMOOTH, dt);
    c->fov = Approach(c->fov, wantFov, CAM_SMOOTH, dt);
    c->yaw = yaw;

    c->eye = (Vector3){ c->focus.x + sinf(yaw)*c->dist,
                        c->focus.y + tanf(c->pitch)*c->dist,
                        c->focus.z + cosf(yaw)*c->dist };
    c->cam.target = c->focus;
    c->cam.position = c->eye;
    c->cam.fovy = c->fov;
}
