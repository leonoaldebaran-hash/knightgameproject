#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "raymath.h"

#define SCREEN_W 640
#define SCREEN_H 480

typedef enum PlayerState {
    PSTATE_NORMAL,   // andando livre
    PSTATE_ATTACK,   // golpe de espada em andamento
    PSTATE_SHIELD,   // escudo levantado
    PSTATE_HURT,     // empurrado para trás depois de levar um golpe
} PlayerState;

typedef struct Player {
    Vector3 pos;
    Vector3 vel;
    float angle;        // direção para onde o jogador olha (radianos)
    float radius;       // raio usado na colisão
    PlayerState state;
    int attackStep;     // golpe atual do combo (0, 1 ou 2)
    float attackTimer;  // tempo desde o início do golpe
    int attackId;       // muda a cada golpe, para cada golpe acertar um alvo só uma vez
    int hp, maxHp;      // vida em meios corações (2 = 1 coração)
    float hurtTimer;    // tempo no estado de dano
    float invulnTimer;  // tempo restante sem poder levar dano (pisca)
    float blockFlash;   // brilho no escudo depois de defender
    float fallenTimer;  // tempo da mensagem "Voce caiu!"
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
bool Input_Attack(void);
bool Input_Shield(void);

// Mundo (world.c)
void World_Init(void);
Vector3 World_Collide(Vector3 pos, float radius);
void World_Draw(void);

// Jogador (player.c)
void Player_Init(Player *p);
void Player_Update(Player *p, float camYaw, float dt);
void Player_Draw(const Player *p);
typedef enum { HIT_IGNORED, HIT_BLOCKED, HIT_TAKEN } HitResult;
HitResult Player_TakeHit(Player *p, Vector3 from, int damage);

// Soldados inimigos (soldier.c)
void Soldiers_Init(void);
void Soldiers_Update(Player *p, float dt);
int Soldiers_Hit(Vector3 origin, float angle, float reach, float halfArc, int attackId);
Vector3 Soldiers_Collide(Vector3 pos, float radius);
void Soldiers_Draw(void);

// Bonecos de treino (dummy.c)
void Dummies_Init(void);
void Dummies_Update(float dt);
int Dummies_Hit(Vector3 origin, float angle, float reach, float halfArc, int attackId);
Vector3 Dummies_Collide(Vector3 pos, float radius);
void Dummies_Draw(void);
int Dummies_TotalHits(void);

// Câmera (camera.c)
void GameCamera_Init(GameCamera *c);
void GameCamera_Update(GameCamera *c, const Player *p, float dt);

#endif
