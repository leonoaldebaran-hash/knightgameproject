#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "raymath.h"

#define SCREEN_W 640
#define SCREEN_H 480

typedef struct Player {
    Vector3 pos;
    Vector3 vel;
    float angle;    // direção para onde o jogador olha (radianos)
    float radius;   // raio usado na colisão
} Player;

typedef struct GameCamera {
    Camera3D cam;
    float yaw;      // ângulo da câmera em volta do jogador
    Vector3 focus;  // ponto que a câmera segue
} GameCamera;

// Deixa o ângulo entre -PI e PI, para girar sempre pelo caminho mais curto
static inline float WrapAngle(float a)
{
    while (a > PI) a -= 2.0f*PI;
    while (a < -PI) a += 2.0f*PI;
    return a;
}

// Entrada (input.c)
Vector2 Input_Move(void);
float Input_Orbit(void);
bool Input_Recenter(void);

// Mundo (world.c)
void World_Init(void);
Vector3 World_Collide(Vector3 pos, float radius);
void World_Draw(void);

// Jogador (player.c)
void Player_Init(Player *p);
void Player_Update(Player *p, float camYaw, float dt);
void Player_Draw(const Player *p);

// Câmera (camera.c)
void GameCamera_Init(GameCamera *c);
void GameCamera_Update(GameCamera *c, const Player *p, float dt);

#endif
