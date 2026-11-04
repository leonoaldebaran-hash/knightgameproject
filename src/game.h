#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "raymath.h"

#define SCREEN_W 640
#define SCREEN_H 480

// Conversão dos números do Ocarina of Time (estudados no Ship of Harkinian) para o nosso mundo.
// O Link adulto mede 68 unidades; o Mateo ~1,75 m. O Ocarina roda a lógica a 20 quadros por segundo,
// então velocidade "por quadro" vira "por segundo" multiplicando por 20, e aceleração por 20².
#define OOT_UNIT        0.026f
#define OOT_FPS         20.0f
#define OOT_DIST(u)     ((u)*OOT_UNIT)                       // distância
#define OOT_SPEED(u)    ((u)*OOT_FPS*OOT_UNIT)               // unidades/quadro  -> m/s
#define OOT_ACCEL(u)    ((u)*OOT_FPS*OOT_FPS*OOT_UNIT)       // unidades/quadro² -> m/s²
#define OOT_TURN(s16)   ((s16)/65536.0f*2.0f*PI*OOT_FPS)     // ângulo s16/quadro -> rad/s
#define OOT_FRAMES(n)   ((n)/OOT_FPS)                        // quadros -> segundos

// ---------------------------------------------------------------------------
// Atores (actor.c), no modelo do Ocarina: tudo que vive no mundo é um ator, com
// categoria, marcadores e funções de iniciar/atualizar/desenhar.
// ---------------------------------------------------------------------------
typedef enum {
    ACTORCAT_SWITCH, ACTORCAT_BG, ACTORCAT_PLAYER, ACTORCAT_EXPLOSIVE, ACTORCAT_NPC,
    ACTORCAT_ENEMY, ACTORCAT_PROP, ACTORCAT_ITEMACTION, ACTORCAT_MISC, ACTORCAT_BOSS,
    ACTORCAT_DOOR, ACTORCAT_CHEST, ACTORCAT_MAX
} ActorCategory;

#define ACTOR_FLAG_TARGETABLE (1 << 0)   // pode ser travado com a mira
#define ACTOR_FLAG_HOSTILE    (1 << 2)   // inimigo (seta amarela, música de combate)
#define ACTOR_FLAG_FRIENDLY   (1 << 3)

// Tipos de dano: quem bate diz o tipo, e a tabela de cada ator diz quanto tira
typedef enum { DMG_SLASH, DMG_SPIN, DMG_JUMP, DMG_ENEMY_SWORD, DMG_COUNT } DamageType;
#define DMG_BIT(t) (1u << (t))

#define MASS_IMMOVABLE 255   // não é empurrado por nada
#define MASS_HEAVY     254   // só empurra quem é mais leve

typedef struct Actor Actor;

typedef struct ActorProfile {
    const char *name;
    ActorCategory category;
    unsigned flags;
    int size;                               // tamanho da struct do ator (que começa com Actor)
    void (*init)(Actor *a);
    void (*update)(Actor *a, float dt);
    void (*draw)(Actor *a);
} ActorProfile;

typedef struct ColChkInfo {
    int health;
    unsigned char mass;
    const unsigned char *damageTable;       // dano por DamageType (NULL = 1 de dano para tudo)
    Vector3 displacement;                   // empurrão acumulado nas colisões OC do quadro
} ColChkInfo;

struct Actor {
    const ActorProfile *profile;
    ActorCategory category;
    unsigned flags;
    Vector3 pos, home;
    float yaw;
    Vector3 vel;
    float xzDistToPlayer, yawTowardsPlayer;
    float targetHeight;                     // altura do marcador da mira
    ColChkInfo colChk;
    bool killed;
    int params;
    Actor *next;
};

// ---------------------------------------------------------------------------
// Colisores (collision.c), no modelo do Ocarina:
// AT = ataque, AC = corpo que leva dano, OC = corpos que se empurram.
// ---------------------------------------------------------------------------
typedef enum { COLSHAPE_CYLINDER, COLSHAPE_QUAD } ColShape;

#define AT_ON          (1 << 0)
#define AT_HIT         (1 << 1)   // acertou algo neste quadro
#define AT_BOUNCED     (1 << 2)   // bateu num colisor duro (escudo) e ricocheteou
#define AT_TYPE_PLAYER (1 << 3)   // dano do lado do jogador
#define AT_TYPE_ENEMY  (1 << 4)   // dano do lado dos inimigos

#define AC_ON          (1 << 0)
#define AC_HIT         (1 << 1)   // levou um golpe neste quadro
#define AC_HARD        (1 << 2)   // duro: golpes ricocheteiam (escudo)
#define AC_TYPE_PLAYER (1 << 3)   // leva dano de ataques do jogador
#define AC_TYPE_ENEMY  (1 << 4)   // leva dano de ataques de inimigos
#define AC_BOUNCED     (1 << 5)   // fez um golpe ricochetear

#define OC_ON          (1 << 0)
#define OC_HIT         (1 << 1)

typedef struct Collider {
    Actor *actor;
    ColShape shape;
    unsigned char atFlags, acFlags, ocFlags;
    unsigned touchDmg;          // AT: tipos de dano que causa
    int damage;                 // AT: dano base (usado quando quem leva não tem tabela)
    unsigned bumpDmg;           // AC: tipos de dano que aceita
    float radius, height;       // cilindro (base em pos)
    Vector3 pos;
    Vector3 quad[4];            // quadrilátero: base e ponta agora, base e ponta no quadro anterior
    bool quadFresh;             // próximo SetQuad começa uma varredura nova
    Actor *atHit, *acHit;       // resultados do quadro: quem acertou / quem bateu
    unsigned acHitDmg;          // tipos de dano do golpe recebido
    int acHitDamage;
} Collider;

typedef enum PlayerState {
    PSTATE_NORMAL,   // andando livre
    PSTATE_ATTACK,   // golpe de espada em andamento
    PSTATE_SHIELD,   // escudo levantado
    PSTATE_HURT,     // empurrado para trás depois de levar um golpe
    PSTATE_DODGE,    // salto lateral ou mortal para trás (com a mira travada)
    PSTATE_ROLL,     // rolamento para a frente (sem a mira)
    PSTATE_CHARGE,   // segurando o botão de ataque: carregando o golpe giratório
    PSTATE_SPIN,     // golpe giratório
    PSTATE_CLIMB,    // subindo numa borda (escalada baixa, alta ou pulando para agarrar)
    PSTATE_JUMPATTACK, // golpe com pulo (mira travada + ação para a frente)
} PlayerState;

typedef struct Player {
    Actor actor;        // o jogador também é um ator (categoria PLAYER), como no Ocarina
    Collider swordAT;   // lâmina varrendo o espaço durante o golpe
    Collider bodyAC;    // corpo que leva dano
    Collider shieldAC;  // escudo levantado: duro, golpes ricocheteiam
    Collider bodyOC;    // corpo que empurra e é empurrado
    bool swordActive;   // a lâmina estava machucando no quadro anterior
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
    float dodgeTimer;   // tempo desde o início da esquiva
    bool dodgeBack;     // esquiva para trás (mortal) ou para o lado
    float dodgeSide;    // -1 esquerda, +1 direita (só no salto lateral)
    float rollTimer;    // tempo desde o início do rolamento
    float chargeTime;   // tempo carregando o golpe giratório
    float spinTimer;    // tempo desde o início do golpe giratório
    float velY;         // velocidade vertical (pulos e quedas)
    bool grounded;      // pisando em algum chão
    float groundY;      // altura do chão embaixo do jogador (para a sombra)
    float ledgeTimer;   // tempo empurrando uma parede com borda (para subir nela)
    Vector3 climbFrom, climbTo;   // escalada: de onde sai e onde termina (em cima da borda)
    float climbTimer, climbTime;
    float jumpAttackTimer;
    float landTimer;    // tempo desde que o golpe com pulo aterrissou (-1 = ainda no ar)
} Player;

typedef struct GameCamera {
    Camera3D cam;
    float yaw;      // ângulo da câmera em volta do jogador
    float dist;     // distância horizontal até o foco
    float pitch;    // inclinação para baixo (radianos)
    float fov;      // campo de visão vertical (graus)
    Vector3 eye;    // posição da câmera no mundo
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
bool Input_Target(void);
bool Input_TargetPressed(void);
bool Input_Dodge(void);
bool Input_Attack(void);
bool Input_AttackHeld(void);
bool Input_Shield(void);

// Mundo (world.c)
#define STEP_UP 0.45f   // degrau mais alto que se sobe só andando
void World_Init(void);
Vector3 World_Collide(Vector3 pos, float radius);
float World_GroundHeight(Vector3 pos);
bool World_LedgeAhead(Vector3 pos, float angle, float *top, Vector3 *standPoint);
void World_Draw(void);

// Jogador (player.c)
void Player_Init(Player *p);
void Player_Update(Player *p, float camYaw, float dt);
void Player_AfterCollision(Player *p);   // aplica o empurrão das colisões OC
void Player_Draw(const Player *p);

// Atores (actor.c)
Actor *Actor_Spawn(const ActorProfile *profile, Vector3 pos, float yaw, int params);
void Actor_Register(Actor *a, ActorCategory category);   // ator que não foi criado por Actor_Spawn (o jogador)
void Actor_Kill(Actor *a);
Actor *Actor_First(ActorCategory category);
void Actor_UpdateAll(const Player *p, float dt);
void Actor_ApplyDisplacement(void);
void Actor_DrawAll(void);
int Actor_ApplyDamage(Actor *a, unsigned dmgFlags, int baseDamage);   // tira vida pela tabela; retorna o dano
float Actor_NearestHostileDist(Vector3 pos);   // -1 se nenhum

// Colisões (collision.c)
void Collider_InitCylinder(Collider *c, Actor *actor, float radius, float height);
void Collider_InitQuad(Collider *c, Actor *actor);
void Collider_SetQuad(Collider *c, Vector3 base, Vector3 tip);
void CollisionCheck_Clear(void);
void CollisionCheck_SetAT(Collider *c);
void CollisionCheck_SetAC(Collider *c);
void CollisionCheck_SetOC(Collider *c);
void CollisionCheck_Run(void);

// Atores do jogo
extern const ActorProfile EnSoldier_Profile;   // soldado de armadura (soldier.c)
extern const ActorProfile EnDummy_Profile;     // boneco de treino (dummy.c)
int Dummies_TotalHits(void);

// Mira travada (target.c)
void Target_Update(const Player *p, const Camera3D *cam, float dt);
bool Target_Active(void);
Vector3 Target_Pos(void);
void Target_Draw(void);
float Target_BarAmount(void);   // 0 a 1: faixas pretas de cinema na tela

// Música (music.c): uma faixa por lugar, mais a de combate
typedef enum { TRACK_FIELD, TRACK_BATTLE, TRACK_COUNT } TrackId;
void Music_Init(void);
void Music_SetArea(TrackId track);
void Music_Update(float enemyDist, bool playerStill, float dt);   // enemyDist < 0 = nenhum inimigo

// Sequenciador no estilo N64 tocando orquestra gravada (synth.c).
// A ordem dos instrumentos do banco é a mesma de tools/build_soundbank.py.
typedef enum {
    INST_HORN, INST_TRUMPET, INST_TROMBONE, INST_TUBA,
    INST_VIOLINS, INST_VIOLINS_SHORT, INST_CELLOS, INST_CELLOS_SHORT, INST_CONTRABASS,
    INST_HARP, INST_FLUTE, INST_OBOE, INST_OCARINA, INST_GLOCK,
    INST_TIMPANI, INST_SNARE, INST_BASSDRUM, INST_CYMBAL, INST_WARDRUM, INST_GONG,
    BANK_COUNT,
    INST_CHOIR = BANK_COUNT,   // coral sintetizado (não há coral gravado livre)
    INST_COUNT
} InstrumentId;

typedef struct SongChannel {
    InstrumentId instrument;
    float volume;
    int layer;          // 0 = toca sempre; 1 = só entra com intensidade (combate)
    const char *mml;    // as notas, em MML
    float pan;          // -1 esquerda, 0 centro, 1 direita
} SongChannel;

typedef struct Song {
    const char *name;
    float bpm;
    int channelCount;
    SongChannel channels[24];
} Song;

bool Synth_LoadBank(const unsigned char *data, int size);
bool Synth_BankLoaded(void);
void Synth_Init(void);
void Synth_Play(const Song *song);
void Synth_SetIntensity(float amount);
void Synth_SetVolume(float volume);
bool Synth_ToggleMute(void);   // retorna true se ficou mudo

// Músicas compostas (songs.c)
extern const Song SONG_FIELD;

// Efeitos sonoros (sfx.c)
typedef enum {
    SFX_SWING, SFX_HIT, SFX_BLOCK, SFX_HURT, SFX_ALERT,
    SFX_VOICE_ATTACK,   // grito curto do Mateo ao golpear ("rá!")
    SFX_VOICE_HURT,     // gemido do Mateo ao levar dano
    SFX_VOICE_ENEMY,    // grunhido grave do soldado ao atacar
    SFX_COUNT
} SfxId;
void Sfx_Init(void);
void Sfx_Play(SfxId id);

// Câmera (camera.c)
void GameCamera_Init(GameCamera *c);
void GameCamera_Update(GameCamera *c, const Player *p, float dt);

#endif
