#include "game.h"

// Checagem de colisões no modelo do Ocarina (estudado no Ship of Harkinian):
// a cada quadro os atores registram seus colisores; depois de todos atualizarem,
// CollisionCheck_Run cruza ataques (AT) com corpos (AC) e os corpos que se empurram (OC).
// O resultado (AT_HIT, AC_HIT, AT_BOUNCED...) fica nos colisores para o dono ler no quadro seguinte.

#define MAX_COLLIDERS 64

static Collider *atList[MAX_COLLIDERS], *acList[MAX_COLLIDERS], *ocList[MAX_COLLIDERS];
static int atCount, acCount, ocCount;

void Collider_InitCylinder(Collider *c, Actor *actor, float radius, float height)
{
    *c = (Collider){ .actor = actor, .shape = COLSHAPE_CYLINDER, .radius = radius, .height = height };
}

void Collider_InitQuad(Collider *c, Actor *actor)
{
    *c = (Collider){ .actor = actor, .shape = COLSHAPE_QUAD, .quadFresh = true };
}

// A lâmina forma um quadrilátero com a posição do quadro anterior: assim um golpe rápido
// não "atravessa" o alvo entre dois quadros
void Collider_SetQuad(Collider *c, Vector3 base, Vector3 tip)
{
    if (c->quadFresh) { c->quad[2] = base; c->quad[3] = tip; c->quadFresh = false; }
    else { c->quad[2] = c->quad[0]; c->quad[3] = c->quad[1]; }
    c->quad[0] = base;
    c->quad[1] = tip;
}

void CollisionCheck_Clear(void)
{
    atCount = acCount = ocCount = 0;
}

void CollisionCheck_SetAT(Collider *c) { if (atCount < MAX_COLLIDERS) atList[atCount++] = c; }
void CollisionCheck_SetAC(Collider *c) { if (acCount < MAX_COLLIDERS) acList[acCount++] = c; }
void CollisionCheck_SetOC(Collider *c) { if (ocCount < MAX_COLLIDERS) ocList[ocCount++] = c; }

static bool PointInCylinder(Vector3 p, const Collider *c)
{
    float dx = p.x - c->pos.x, dz = p.z - c->pos.z;
    return dx*dx + dz*dz <= c->radius*c->radius && p.y >= c->pos.y && p.y <= c->pos.y + c->height;
}

static bool CylinderVsCylinder(const Collider *a, const Collider *b)
{
    float dx = a->pos.x - b->pos.x, dz = a->pos.z - b->pos.z;
    float r = a->radius + b->radius;
    if (dx*dx + dz*dz > r*r) return false;
    return a->pos.y <= b->pos.y + b->height && b->pos.y <= a->pos.y + a->height;
}

// Quadrilátero contra cilindro: testa uma grade de pontos sobre a área varrida pela lâmina
static bool QuadVsCylinder(const Collider *q, const Collider *cyl)
{
    for (int i = 0; i <= 4; i++)
    {
        float u = i/4.0f;
        Vector3 now = Vector3Lerp(q->quad[0], q->quad[1], u);
        Vector3 before = Vector3Lerp(q->quad[2], q->quad[3], u);
        for (int j = 0; j <= 4; j++)
            if (PointInCylinder(Vector3Lerp(now, before, j/4.0f), cyl)) return true;
    }
    return false;
}

static bool Touches(const Collider *at, const Collider *ac)
{
    if (ac->shape != COLSHAPE_CYLINDER) return false;
    return (at->shape == COLSHAPE_QUAD) ? QuadVsCylinder(at, ac) : CylinderVsCylinder(at, ac);
}

// Ataque do jogador só acerta quem leva dano do jogador, e o mesmo para inimigos
static bool Aligned(const Collider *at, const Collider *ac)
{
    return ((at->atFlags & AT_TYPE_PLAYER) && (ac->acFlags & AC_TYPE_PLAYER)) ||
           ((at->atFlags & AT_TYPE_ENEMY) && (ac->acFlags & AC_TYPE_ENEMY));
}

static void CheckAT(void)
{
    for (int i = 0; i < atCount; i++)
    {
        Collider *at = atList[i];
        if (!(at->atFlags & AT_ON)) continue;

        Collider *hits[MAX_COLLIDERS];
        int hitCount = 0;
        bool bounced = false;

        for (int j = 0; j < acCount && !bounced; j++)
        {
            Collider *ac = acList[j];
            if (!(ac->acFlags & AC_ON) || ac->actor == at->actor || !Aligned(at, ac)) continue;
            if (!(at->touchDmg & ac->bumpDmg) || !Touches(at, ac)) continue;

            // Colisor duro (escudo): o golpe ricocheteia e não acerta mais nada
            if (ac->acFlags & AC_HARD)
            {
                at->atFlags |= AT_BOUNCED;
                ac->acFlags |= AC_BOUNCED;
                ac->acHit = at->actor;
                bounced = true;
            }
            else hits[hitCount++] = ac;
        }
        if (bounced) continue;

        for (int k = 0; k < hitCount; k++)
        {
            Collider *ac = hits[k];
            at->atFlags |= AT_HIT;
            at->atHit = ac->actor;
            ac->acFlags |= AC_HIT;
            ac->acHit = at->actor;
            ac->acHitDmg = at->touchDmg;
            ac->acHitDamage = at->damage;
        }
    }
}

// Corpos que se sobrepõem se afastam, dividindo o empurrão pela massa de cada um
static void CheckOC(void)
{
    for (int i = 0; i < ocCount; i++)
        for (int j = i + 1; j < ocCount; j++)
        {
            Collider *l = ocList[i], *r = ocList[j];
            if (!(l->ocFlags & OC_ON) || !(r->ocFlags & OC_ON) || l->actor == r->actor) continue;
            if (!CylinderVsCylinder(l, r)) continue;

            float dx = r->pos.x - l->pos.x, dz = r->pos.z - l->pos.z;
            float dist = sqrtf(dx*dx + dz*dz);
            float overlap = l->radius + r->radius - dist;
            unsigned char lm = l->actor->colChk.mass, rm = r->actor->colChk.mass;
            float lRatio, rRatio;

            if (lm == MASS_IMMOVABLE && rm == MASS_IMMOVABLE) continue;
            if (lm == MASS_IMMOVABLE || (lm == MASS_HEAVY && rm != MASS_IMMOVABLE && rm != MASS_HEAVY)) { lRatio = 0.0f; rRatio = 1.0f; }
            else if (rm == MASS_IMMOVABLE || (rm == MASS_HEAVY && lm != MASS_HEAVY)) { lRatio = 1.0f; rRatio = 0.0f; }
            else if (lm == MASS_HEAVY && rm == MASS_HEAVY) { lRatio = 0.5f; rRatio = 0.5f; }
            else
            {
                float total = (float)lm + (float)rm;
                lRatio = rm/total;
                rRatio = lm/total;
            }

            if (dist < 0.0001f) { dx = 1.0f; dz = 0.0f; dist = 1.0f; }
            dx *= overlap/dist;
            dz *= overlap/dist;
            l->actor->colChk.displacement.x -= dx*lRatio;
            l->actor->colChk.displacement.z -= dz*lRatio;
            r->actor->colChk.displacement.x += dx*rRatio;
            r->actor->colChk.displacement.z += dz*rRatio;
            l->ocFlags |= OC_HIT;
            r->ocFlags |= OC_HIT;
        }
}

void CollisionCheck_Run(void)
{
    CheckAT();
    CheckOC();
}
