#include "game.h"

#define FIELD_SIZE 19.0f   // limite do campo
#define MAX_OBSTACLES 32

typedef enum { SHAPE_BOX, SHAPE_TREE } ShapeType;

typedef struct Obstacle {
    ShapeType type;
    Vector3 pos;    // centro da base (no chão)
    Vector3 size;   // caixa: largura, altura, profundidade | árvore: x = raio do tronco, y = altura
} Obstacle;

static Obstacle obstacles[MAX_OBSTACLES];
static int obstacleCount = 0;

static void AddBox(Vector3 pos, Vector3 size)
{
    if (obstacleCount < MAX_OBSTACLES) obstacles[obstacleCount++] = (Obstacle){ SHAPE_BOX, pos, size };
}

static void AddTree(Vector3 pos, float radius, float height)
{
    if (obstacleCount < MAX_OBSTACLES) obstacles[obstacleCount++] = (Obstacle){ SHAPE_TREE, pos, (Vector3){ radius, height, radius } };
}

void World_Init(void)
{
    obstacleCount = 0;

    // Anel de pilares de pedra
    for (int i = 0; i < 8; i++)
    {
        float a = i*(2.0f*PI/8.0f);
        AddBox((Vector3){ sinf(a)*12.0f, 0.0f, cosf(a)*12.0f }, (Vector3){ 1.0f, 3.0f, 1.0f });
    }

    // Muro baixo para testar o deslize na parede
    AddBox((Vector3){ -5.0f, 0.0f, 3.0f }, (Vector3){ 6.0f, 1.5f, 0.6f });

    AddTree((Vector3){ 4.0f, 0.0f, -4.0f }, 0.3f, 2.0f);
}

// Empurra um círculo (o jogador visto de cima) para fora dos obstáculos.
// Como só a parte "dentro" é corrigida, o jogador desliza ao longo das paredes.
Vector3 World_Collide(Vector3 pos, float radius)
{
    for (int i = 0; i < obstacleCount; i++)
    {
        Obstacle *o = &obstacles[i];

        if (o->type == SHAPE_BOX)
        {
            float minX = o->pos.x - o->size.x/2.0f, maxX = o->pos.x + o->size.x/2.0f;
            float minZ = o->pos.z - o->size.z/2.0f, maxZ = o->pos.z + o->size.z/2.0f;

            // Ponto da caixa mais próximo do jogador
            float cx = Clamp(pos.x, minX, maxX);
            float cz = Clamp(pos.z, minZ, maxZ);
            float dx = pos.x - cx, dz = pos.z - cz;
            float dist2 = dx*dx + dz*dz;

            if (dist2 >= radius*radius) continue;

            if (dist2 > 0.0001f)
            {
                float dist = sqrtf(dist2);
                pos.x = cx + dx/dist*radius;
                pos.z = cz + dz/dist*radius;
            }
            else
            {
                // Centro dentro da caixa: sai pelo lado mais próximo
                float left = pos.x - minX, right = maxX - pos.x;
                float back = pos.z - minZ, front = maxZ - pos.z;
                float m = fminf(fminf(left, right), fminf(back, front));
                if (m == left) pos.x = minX - radius;
                else if (m == right) pos.x = maxX + radius;
                else if (m == back) pos.z = minZ - radius;
                else pos.z = maxZ + radius;
            }
        }
        else
        {
            float dx = pos.x - o->pos.x, dz = pos.z - o->pos.z;
            float minDist = radius + o->size.x;
            float dist2 = dx*dx + dz*dz;

            if (dist2 >= minDist*minDist) continue;

            if (dist2 > 0.0001f)
            {
                float dist = sqrtf(dist2);
                pos.x = o->pos.x + dx/dist*minDist;
                pos.z = o->pos.z + dz/dist*minDist;
            }
            else pos.x = o->pos.x + minDist;
        }
    }

    pos.x = Clamp(pos.x, -FIELD_SIZE, FIELD_SIZE);
    pos.z = Clamp(pos.z, -FIELD_SIZE, FIELD_SIZE);
    return pos;
}

void World_Draw(void)
{
    DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 40.0f, 40.0f }, (Color){ 90, 140, 70, 255 });

    for (int i = 0; i < obstacleCount; i++)
    {
        Obstacle *o = &obstacles[i];

        if (o->type == SHAPE_BOX)
        {
            Vector3 center = { o->pos.x, o->pos.y + o->size.y/2.0f, o->pos.z };
            DrawCube(center, o->size.x, o->size.y, o->size.z, GRAY);
            DrawCubeWires(center, o->size.x, o->size.y, o->size.z, DARKGRAY);
        }
        else
        {
            DrawCylinder(o->pos, o->size.x, o->size.x, o->size.y, 8, BROWN);
            DrawSphere((Vector3){ o->pos.x, o->pos.y + o->size.y + 0.6f, o->pos.z }, 1.2f, DARKGREEN);
        }
    }
}
