#include "game.h"
#include "rlgl.h"
#include <stddef.h>

#define TARGET_RANGE   10.0f   // distância máxima para travar a mira
#define LOSE_RANGE     14.0f   // distância em que a mira solta sozinha
#define VIEW_LIMIT     (100.0f*DEG2RAD)   // só alvos à frente da câmera
#define BAR_SPEED      5.0f

static Actor *target = NULL;   // ator travado na mira
static Vector3 pos;
static Actor *hint = NULL;     // ator que seria travado agora (marcador fraco)
static float bars = 0.0f;

static bool Targetable(const Actor *a)
{
    return a && !a->killed && (a->flags & ACTOR_FLAG_TARGETABLE);
}

// Escolhe o melhor alvo entre os atores que podem ser mirados:
// perto do jogador, à frente da câmera, e inimigos antes de objetos
static Actor *FindBest(const Player *p, const Camera3D *cam)
{
    float camYaw = atan2f(cam->target.x - cam->position.x, cam->target.z - cam->position.z);
    float bestScore = 1e9f;
    Actor *best = NULL;

    for (int c = 0; c < ACTORCAT_MAX; c++)
        for (Actor *a = Actor_First(c); a; a = a->next)
        {
            if (c == ACTORCAT_PLAYER || !Targetable(a)) continue;

            float dx = a->pos.x - p->pos.x, dz = a->pos.z - p->pos.z;
            float dist = sqrtf(dx*dx + dz*dz);
            if (dist > TARGET_RANGE) continue;

            float off = fabsf(WrapAngle(atan2f(dx, dz) - camYaw));
            if (off > VIEW_LIMIT) continue;

            float score = dist + off*3.0f + ((a->flags & ACTOR_FLAG_HOSTILE) ? 0.0f : 3.0f);
            if (score < bestScore) { bestScore = score; best = a; }
        }
    return best;
}

void Target_Update(const Player *p, const Camera3D *cam, float dt)
{
    if (!target && Input_TargetPressed()) target = FindBest(p, cam);

    if (target)
    {
        // Soltar o botão, o alvo cair ou ficar longe demais desfaz a mira
        if (!Input_Target() || !Targetable(target) || Vector3Distance(p->pos, target->pos) > LOSE_RANGE) target = NULL;
        else pos = target->pos;
    }

    hint = target ? NULL : FindBest(p, cam);

    // Faixas de cinema com a mira travada e também com a mira segurada sem alvo, como no Ocarina
    float wanted = (target || Input_Target()) ? 1.0f : 0.0f;
    if (bars < wanted) bars = fminf(wanted, bars + BAR_SPEED*dt);
    else bars = fmaxf(wanted, bars - BAR_SPEED*dt);
}

bool Target_Active(void)
{
    return target != NULL;
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
static void DrawMarker(const Actor *a, float size, Color color)
{
    Vector3 at = a->pos;
    float t = (float)GetTime();
    float y = at.y + a->targetHeight + sinf(t*6.0f)*0.1f;
    rlPushMatrix();
        rlTranslatef(at.x, y, at.z);
        rlRotatef(t*180.0f, 0.0f, 1.0f, 0.0f);
        DrawCylinderEx((Vector3){ 0.0f, size, 0.0f }, (Vector3){ 0.0f, -size, 0.0f }, size, 0.0f, 4, color);
        DrawCylinderWiresEx((Vector3){ 0.0f, size, 0.0f }, (Vector3){ 0.0f, -size, 0.0f }, size, 0.0f, 4, Fade(BLACK, 0.5f));
    rlPopMatrix();
}

void Target_Draw(void)
{
    // Seta amarela para inimigos e azul para o resto, como no Ocarina
    if (target) DrawMarker(target, 0.28f, (target->flags & ACTOR_FLAG_HOSTILE) ? YELLOW : SKYBLUE);
    else if (hint) DrawMarker(hint, 0.15f, Fade(WHITE, 0.6f));
}
