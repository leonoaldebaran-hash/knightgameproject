#include "game.h"
#include <stdlib.h>

// Sistema de atores no modelo do Ocarina (estudado no Ship of Harkinian): uma lista por
// categoria; cada ator é criado a partir de um "perfil" com suas funções; atores mortos são
// removidos no fim da atualização.

static Actor *lists[ACTORCAT_MAX];

Actor *Actor_Spawn(const ActorProfile *profile, Vector3 pos, float yaw, int params)
{
    Actor *a = calloc(1, (size_t)profile->size);
    if (!a) return NULL;

    a->profile = profile;
    a->category = profile->category;
    a->flags = profile->flags;
    a->pos = a->home = pos;
    a->yaw = yaw;
    a->params = params;
    a->targetHeight = 2.2f;
    a->colChk.mass = 50;
    a->next = lists[a->category];
    lists[a->category] = a;

    if (profile->init) profile->init(a);
    return a;
}

void Actor_Register(Actor *a, ActorCategory category)
{
    a->category = category;
    a->next = lists[category];
    lists[category] = a;
}

void Actor_Kill(Actor *a)
{
    a->killed = true;
}

Actor *Actor_First(ActorCategory category)
{
    return lists[category];
}

int Actor_ApplyDamage(Actor *a, unsigned dmgFlags, int baseDamage)
{
    int damage = baseDamage;
    if (a->colChk.damageTable)
        for (int t = 0; t < DMG_COUNT; t++)
            if (dmgFlags & DMG_BIT(t)) { damage = a->colChk.damageTable[t]; break; }
    a->colChk.health -= damage;
    return damage;
}

void Actor_UpdateAll(const Player *p, float dt)
{
    for (int c = 0; c < ACTORCAT_MAX; c++)
    {
        for (Actor *a = lists[c]; a; a = a->next)
        {
            if (a->killed || !a->profile || !a->profile->update) continue;
            float dx = p->pos.x - a->pos.x, dz = p->pos.z - a->pos.z;
            a->xzDistToPlayer = sqrtf(dx*dx + dz*dz);
            a->yawTowardsPlayer = atan2f(dx, dz);
            a->profile->update(a, dt);
        }

        // Remove os atores mortos (só os criados por Actor_Spawn são liberados)
        Actor **link = &lists[c];
        while (*link)
        {
            Actor *a = *link;
            if (a->killed && a->profile) { *link = a->next; free(a); }
            else link = &a->next;
        }
    }
}

// Aplica o empurrão acumulado nas colisões OC e mantém o ator fora das paredes
// (o jogador aplica o seu em Player_AfterCollision)
void Actor_ApplyDisplacement(void)
{
    for (int c = 0; c < ACTORCAT_MAX; c++)
    {
        if (c == ACTORCAT_PLAYER) continue;
        for (Actor *a = lists[c]; a; a = a->next)
        {
            if (a->colChk.displacement.x == 0.0f && a->colChk.displacement.z == 0.0f) continue;
            a->pos = Vector3Add(a->pos, a->colChk.displacement);
            a->pos = World_Collide(a->pos, 0.4f);
            a->colChk.displacement = Vector3Zero();
        }
    }
}

void Actor_DrawAll(void)
{
    for (int c = 0; c < ACTORCAT_MAX; c++)
        for (Actor *a = lists[c]; a; a = a->next)
            if (!a->killed && a->profile && a->profile->draw) a->profile->draw(a);
}

// Inimigo hostil mais próximo (decide o volume da música de inimigo, como no Ocarina)
float Actor_NearestHostileDist(Vector3 pos)
{
    float best = -1.0f;
    for (Actor *a = lists[ACTORCAT_ENEMY]; a; a = a->next)
    {
        if (a->killed || !(a->flags & ACTOR_FLAG_HOSTILE)) continue;
        float d = Vector3Distance(pos, a->pos);
        if (best < 0.0f || d < best) best = d;
    }
    return best;
}
