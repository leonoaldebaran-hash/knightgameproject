#include "game.h"
#include "rlgl.h"

#define TARGET_RANGE   10.0f   // distância máxima para travar a mira
#define LOSE_RANGE     14.0f   // distância em que a mira solta sozinha
#define VIEW_LIMIT     (100.0f*DEG2RAD)   // só alvos à frente da câmera
#define BAR_SPEED      5.0f

typedef enum { KIND_NONE, KIND_SOLDIER, KIND_DUMMY } TargetKind;

static TargetKind kind = KIND_NONE;
static int targetIndex = 0;
static Vector3 pos;
static bool hasHint = false;   // alvo que seria travado agora (marcador fraco)
static Vector3 hintPos;
static float bars = 0.0f;

static bool GetPos(TargetKind k, int i, Vector3 *out)
{
    if (k == KIND_SOLDIER) return Soldiers_TargetPos(i, out);
    if (k == KIND_DUMMY) return Dummies_TargetPos(i, out);
    return false;
}

// Escolhe o melhor alvo: perto do jogador e o mais à frente possível da câmera
static bool FindBest(const Player *p, const Camera3D *cam, TargetKind *outKind, int *outIndex, Vector3 *outPos)
{
    float camYaw = atan2f(cam->target.x - cam->position.x, cam->target.z - cam->position.z);
    float bestScore = 1e9f;
    bool found = false;

    for (int k = KIND_SOLDIER; k <= KIND_DUMMY; k++)
    {
        int count = (k == KIND_SOLDIER) ? Soldiers_Count() : Dummies_Count();
        for (int i = 0; i < count; i++)
        {
            Vector3 c;
            if (!GetPos(k, i, &c)) continue;

            float dx = c.x - p->pos.x, dz = c.z - p->pos.z;
            float dist = sqrtf(dx*dx + dz*dz);
            if (dist > TARGET_RANGE) continue;

            float off = fabsf(WrapAngle(atan2f(dx, dz) - camYaw));
            if (off > VIEW_LIMIT) continue;

            // Soldados têm prioridade sobre bonecos de treino
            float score = dist + off*3.0f + ((k == KIND_DUMMY) ? 3.0f : 0.0f);
            if (score < bestScore)
            {
                bestScore = score;
                *outKind = k;
                *outIndex = i;
                *outPos = c;
                found = true;
            }
        }
    }
    return found;
}

void Target_Update(const Player *p, const Camera3D *cam, float dt)
{
    TargetKind k;
    int i;
    Vector3 c;

    if (kind == KIND_NONE && Input_TargetPressed() && FindBest(p, cam, &k, &i, &c))
    {
        kind = k;
        targetIndex = i;
        pos = c;
    }

    if (kind != KIND_NONE)
    {
        // Soltar o botão, o alvo cair ou ficar longe demais desfaz a mira
        bool valid = GetPos(kind, targetIndex, &pos);
        if (!Input_Target() || !valid || Vector3Distance(p->pos, pos) > LOSE_RANGE) kind = KIND_NONE;
    }

    hasHint = (kind == KIND_NONE) && FindBest(p, cam, &k, &i, &hintPos);

    float wanted = (kind != KIND_NONE) ? 1.0f : 0.0f;
    if (bars < wanted) bars = fminf(wanted, bars + BAR_SPEED*dt);
    else bars = fmaxf(wanted, bars - BAR_SPEED*dt);
}

bool Target_Active(void)
{
    return kind != KIND_NONE;
}

Vector3 Target_Pos(void)
{
    return pos;
}

float Target_BarAmount(void)
{
    return bars;
}

// Pirâmide de cabeça para baixo girando sobre o alvo, como uma seta
static void DrawMarker(Vector3 at, float size, Color color)
{
    float t = (float)GetTime();
    float y = 2.5f + sinf(t*6.0f)*0.1f;
    rlPushMatrix();
        rlTranslatef(at.x, y, at.z);
        rlRotatef(t*180.0f, 0.0f, 1.0f, 0.0f);
        DrawCylinderEx((Vector3){ 0.0f, size, 0.0f }, (Vector3){ 0.0f, -size, 0.0f }, size, 0.0f, 4, color);
        DrawCylinderWiresEx((Vector3){ 0.0f, size, 0.0f }, (Vector3){ 0.0f, -size, 0.0f }, size, 0.0f, 4, Fade(BLACK, 0.5f));
    rlPopMatrix();
}

void Target_Draw(void)
{
    if (kind != KIND_NONE) DrawMarker(pos, 0.28f, YELLOW);
    else if (hasHint) DrawMarker(hintPos, 0.15f, Fade(WHITE, 0.6f));
}
