/*
 * Source reconstruction for weapon firing logic.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include "recovered_game.h"
#include "g_public.h"
#include "game_globals.h"
#include "level_locals.h"
#include "g_syscalls.h"
#include "game_functions.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_x87_eval.h"
#include "compat/crt/sin_cos_compat.h"

#define MAX_BULLET_RECURSION 13
#define WEAPON_PROJECTILE_SPREAD_DISTANCE 16.0f
#define BULLET_TRACE_DISTANCE 10000.0f
#define WEAPON_SPREAD_PI 3.141592653589793  /* original double64 0x400921fb54442d18 */
#define WEAPON_SPREAD_DEGREES 180.0         /* original double64 0x4066800000000000 */
#define WEAPON_SPREAD_WINDOWS_PI 3.1415927f
#define WEAPON_SPREAD_WINDOWS_INV_DEGREES 0.0055555557f
#define DAMAGE_FLAG_RICOCHET 0x20
#define MUZZLE_ADS_FORWARD_DISTANCE 19.0f
#define MUZZLE_MIN_VIEW_HEIGHT 8.0f
#define MUZZLE_BOB_CYCLE_DIVISOR 255.0f
#define MUZZLE_BOB_PHASE_TWO_PI 6.2831855f

/* NOT_FROM_ORIGINAL_SOURCE: local vector helper factored from recovered fire-weapon bodies. */
static void game_compat_fire_weapon_vector_copy(const float *src, float *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: local vector helper factored from recovered fire-weapon bodies. */
static void game_compat_fire_weapon_vector_subtract(const float *a, const float *b, float *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

/* NOT_FROM_ORIGINAL_SOURCE: local vector helper factored from recovered fire-weapon bodies. */
static void game_compat_fire_weapon_vector_ma(const float *start, float scale, const float *dir,
                                float *out)
{
    out[0] = start[0] + dir[0] * scale;
    out[1] = start[1] + dir[1] * scale;
    out[2] = start[2] + dir[2] * scale;
}


/* NOT_FROM_ORIGINAL_SOURCE: local predicate factored from recovered tracer checks. */
static qboolean game_compat_fire_weapon_is_tracer_ammo_type(int ammoType)
{
    return ammoType == WEAPON_AMMO_TYPE_LMG ||
           ammoType == WEAPON_AMMO_TYPE_HMG ||
           ammoType == WEAPON_AMMO_TYPE_UMG;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from recovered tracer spawn checks. */
static float game_compat_fire_weapon_tracer_chance(gentity_t *source)
{
    float chance = g_tracerChance.value;
    const weaponInfo_t *weaponInfo =
        (const weaponInfo_t *)BG_GetInfoForWeapon(source->s.weapon);

    if (weaponInfo != NULL && game_compat_fire_weapon_is_tracer_ammo_type(weaponInfo->ammoType)) {
        chance = g_tracerChanceLMG.value;
    }
    return chance;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from recovered tracer spawn code. */
static void game_compat_fire_weapon_spawn_tracer(const float *start, const float *end,
                                   const weaponInfo_t *weaponInfo)
{
    gentity_t *event = G_TempEntity(start, EV_BULLET_TRACER);

    game_compat_fire_weapon_vector_copy(end, event->s.loopedFxForward);
    event->s.tempEffectId = weaponInfo->weaponIndex;
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from recovered projectile spread code. */
static void game_compat_fire_weapon_spread_direction(const weapon_muzzle_t *muzzle, float spread,
                                       float *dir)
{
    float x, y, radius;
    WIDE_FLOAT radians = (WIDE_FLOAT)spread * WEAPON_SPREAD_WINDOWS_PI;
    radians *= WEAPON_SPREAD_WINDOWS_INV_DEGREES;
    /* Windows keeps the tangent live until the scaled radius store. */
    radius = coduo_round_f32(coduo_x87_tanl(radians) * WEAPON_PROJECTILE_SPREAD_DISTANCE);
    gunrandom(&x, &y);

    WIDE_FLOAT lateralRight = (WIDE_FLOAT)x * radius;
    WIDE_FLOAT lateralUp = (WIDE_FLOAT)y * radius;
    /* 0x7b446/0x7b46f/0x7b4a2 (and the artillery copy): Linux stores the
     * forward, right and up stages separately. Multiplication by 16 is exact
     * for the bounded direction components. Windows retains the middle lane
     * through both additions; X and Z keep the intermediate float store. */
    for (int i = 0; i < 3; i++) {
        float forward = muzzle->forward[i] * WEAPON_PROJECTILE_SPREAD_DISTANCE;
        WIDE_FLOAT right = lateralRight * muzzle->right[i] + forward;
        if (i != 1)
        {
            right = coduo_round_f32(right);
        }
        dir[i] = lateralUp * muzzle->up[i] + right;
    }
    VectorNormalize(dir);
}

/* NOT_FROM_ORIGINAL_SOURCE: local helper factored from recovered projectile velocity code. */
static void game_compat_fire_weapon_add_projectile_inherited_velocity(gentity_t *projectile,
                                                      gentity_t *shooter,
                                                      const float *dir)
{
    /* Windows accumulates Z/Y/X and retains the dot product. Linux sums
     * X/Y/Z and stores it as float before the three velocity updates. */
    const int first = 2, last = 0;
    WIDE_FLOAT push = ((WIDE_FLOAT)dir[first] * shooter->client->ps.velocity[first]
                     + (WIDE_FLOAT)dir[1] * shooter->client->ps.velocity[1])
                     + (WIDE_FLOAT)dir[last] * shooter->client->ps.velocity[last];
    projectile->s.pos.trDelta[0] = (WIDE_FLOAT)dir[0] * push + projectile->s.pos.trDelta[0];
    projectile->s.pos.trDelta[1] = (WIDE_FLOAT)dir[1] * push + projectile->s.pos.trDelta[1];
    projectile->s.pos.trDelta[2] = (WIDE_FLOAT)dir[2] * push + projectile->s.pos.trDelta[2];
}

/* ------------------------------------------------------------------ */
/*  0x7a2bc infront                                                   */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a2bc, 8a2bc_infront.c, VERIFY-FIRE-WEAPON-HELPERS-2026-06-17): DATAFLOW_VERIFIED */
qboolean infront(gentity_t *self, gentity_t *other)
{
    vec3_t forward;
    vec3_t dir;

    AngleVectors(self->currentAngles, forward, NULL, NULL);
    game_compat_fire_weapon_vector_subtract(other->currentOrigin, self->currentOrigin, dir);
    VectorNormalize(dir);

    /* Keep the dot products and sum wide, then round once before the test.
     * Windows accumulates Z/Y/X; Linux accumulates X/Y/Z. */
    const int first = 2, last = 0;
    volatile float dot = ((WIDE_FLOAT)dir[first] * forward[first] + (WIDE_FLOAT)dir[1] * forward[1])
                         + (WIDE_FLOAT)dir[last] * forward[last];
    return dot > 0.0f;
}

/* ------------------------------------------------------------------ */
/*  0x7a524 Weapon_Melee                                              */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a524, 8a524_Weapon_Melee.c, VERIFY-FIRE-WEAPON-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void Weapon_Melee(gentity_t *ent, const float *muzzlePoints)
{
    trace_t trace;
    vec3_t end;
    int damage;
    gentity_t *event;
    gentity_t *hitEnt;

    game_compat_fire_weapon_vector_ma(&muzzlePoints[9], 64.0f, muzzlePoints, end);
    trap_LocationalTrace(&trace, &muzzlePoints[9], end, ent->s.number,
                         MASK_BULLETTRACE, bulletPriorityMap);

    damage = ((const weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon))->meleeDamage;
    G_CheckHitTriggerDamage(ent, &muzzlePoints[9], trace.endpos, damage, MOD_MELEE);

    if ((trace.surfaceFlags & 0x10) != 0 || trace.fraction == 1.0f) {
        return;
    }

    hitEnt = &g_entities[trace.entityNum];
    event = G_TempEntity(trace.endpos,
                         hitEnt->client == NULL ? EV_MELEE_MISS : EV_MELEE_HIT);
    event->s.vehicleEntityNum = trace.entityNum;
    event->s.tempEffectId = DirToByte(trace.normal);
    event->s.weapon = ent->s.weapon;

    if (trace.entityNum != ENTITYNUM_WORLD && game_compat_gentity_can_take_damage(hitEnt)) {
        int meleeDamage = coduo_int32_from_bits(
            (uint32_t)coduo_server_randrange(0, 5) + (uint32_t)damage);

        G_Damage(hitEnt, ent, ent, muzzlePoints, trace.endpos,
                 meleeDamage, 0, MOD_MELEE, trace.partGroup);
    }
}

/* ------------------------------------------------------------------ */
/*  0x7a745 SnapVectorTowards                                         */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a745, 8a745_SnapVectorTowards.c, VERIFY-FIRE-WEAPON-HELPERS-2026-06-17): DATAFLOW_VERIFIED */
void SnapVectorTowards(float *point, const float *towards)
{
    for (int axis = 0; axis < 3; axis++) {
        if (towards[axis] <= point[axis]) {
            point[axis] = (float)floor((double)point[axis]);
        } else {
            point[axis] = (float)ceil((double)point[axis]);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x7a7f8 Damage_Falloff                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a7f8, 8a7f8_Damage_Falloff.c, VERIFY-FIRE-WEAPON-PACKET-2026-06-17): DATAFLOW_VERIFIED */
WIDE_FLOAT Damage_Falloff(float distance, float maxDamage, float minDamagePercent,
                     int minRange, int maxRange)
{
    /* Windows stores the minimum range as binary32, then retains fraction
     * and damage through the return. Bullet damage truncates that return;
     * missile impact stores it as binary32 first. Linux consumes integer
     * ranges directly (0x7a810/0x7a843), stores the fraction (0x7a865), and
     * multiplies percent by 0.01 before maxDamage (0x7a877..0x7a882). */
    WIDE_FLOAT minimum = minRange;
    WIDE_FLOAT damage = maxDamage;
    minimum = coduo_round_f32(minimum);
    if (minimum < distance) {
        if ((WIDE_FLOAT)maxRange < distance) {
            damage = (WIDE_FLOAT)minDamagePercent * 0.01f * maxDamage;
        } else {
            int32_t rangeSpan = coduo_int32_from_bits((uint32_t)maxRange - (uint32_t)minRange);
            WIDE_FLOAT fraction = 1.0f - ((WIDE_FLOAT)distance - minimum) / rangeSpan;
            /* Keep multiply and add separate on hosts with fused operations. */
            volatile WIDE_FLOAT scaledDamage = (100.0f - (WIDE_FLOAT)minDamagePercent) * fraction;
            damage = (scaledDamage + minDamagePercent) * maxDamage * 0.01f;
        }
    }

    if (damage < 0.0f) {
        damage = 0.0f;
    }
    return damage;
}

/* ------------------------------------------------------------------ */
/*  0x7a8af Bullet_Fire                                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a8af, 8a8af_Bullet_Fire.c, VERIFY-FIRE-WEAPON-PACKET-2026-06-17): DATAFLOW_VERIFIED - qboolean return preserves the Bullet_Fire_Extended EAX consumed by original vehicle/turret callers. */
qboolean Bullet_Fire(gentity_t *ent, float spread, int damage,
                     weapon_muzzle_t *muzzle, gentity_t *attacker)
{
    vec3_t end;

    BG_Bullet_Endpos(spread, end, muzzle->forward);
    return Bullet_Fire_Extended(attacker, ent, muzzle->origin, end, damage, 0,
                                muzzle, attacker);
}

/* ------------------------------------------------------------------ */
/*  0x7a920 Bullet_Fire_Extended                                      */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7a920, 8a920_Bullet_Fire_Extended.c, VERIFY-FIRE-WEAPON-PACKET-2026-06-17): DATAFLOW_VERIFIED */
qboolean Bullet_Fire_Extended(gentity_t *hitEnt, gentity_t *attacker, float *start,
                              const float *end, int damage, int recursionDepth,
                              weapon_muzzle_t *muzzle, gentity_t *source)
{
    const weaponInfo_t *weaponInfo;
    trace_t trace;
    gentity_t *traceEnt;
    gentity_t *damageInflictor;
    vec3_t dir;
    vec3_t reflected;
    WIDE_FLOAT reflectionScale;
    enum { first = 2, last = 0 };
    int meansOfDeath;
    int damageFlags = 0;
    qboolean hit = qfalse;

    if (recursionDepth >= MAX_BULLET_RECURSION) {
        Com_DPrintf("Bullet_Fire_Extended: Too many resursions, bullet aborted\n");
        return qfalse;
    }

    weaponInfo = muzzle->weaponInfo;

    if (weaponInfo->ricochet == 0) {
        meansOfDeath = MOD_PISTOL_BULLET;
    } else {
        meansOfDeath = MOD_RIFLE_BULLET;
        damageFlags = DAMAGE_FLAG_RICOCHET;
    }

    trap_LocationalTrace(&trace, start, end, hitEnt->s.number,
                         MASK_BULLETTRACE,
                         weaponInfo->ricochet == 0 ? bulletPriorityMap : riflePriorityMap);

    if ((g_debugBullets.integer & 1) != 0) {
        gentity_t *debug = G_TempEntity(start, EV_RAILTRAIL);

        game_compat_fire_weapon_vector_copy(trace.endpos, debug->s.loopedFxForward);
        debug->s.vehicleSlot = attacker->s.number;
    }

    G_CheckHitTriggerDamage(attacker, start, trace.endpos, damage, meansOfDeath);
    traceEnt = &g_entities[trace.entityNum];

    if (trace.fraction < 1.0f) {
        if (g_debugBullets.integer < -1) {
            vec3_t debugStart;
            vec3_t debugEnd;
            gentity_t *debug;

            debugStart[0] = traceEnt->currentOrigin[0] + traceEnt->mins[0];
            debugStart[1] = traceEnt->currentOrigin[1] + traceEnt->mins[1];
            debugStart[2] = traceEnt->currentOrigin[2] + traceEnt->mins[2];
            debugEnd[0] = traceEnt->currentOrigin[0] + traceEnt->maxs[0];
            debugEnd[1] = traceEnt->currentOrigin[1] + traceEnt->maxs[1];
            debugEnd[2] = traceEnt->currentOrigin[2] + traceEnt->maxs[2];
            debug = G_TempEntity(debugStart, EV_RAILTRAIL);
            game_compat_fire_weapon_vector_copy(debugEnd, debug->s.loopedFxForward);
            debug->s.cursorHint = CURSOR_HINT_ACTIVATE;
        }

        game_compat_fire_weapon_vector_subtract(end, start, dir);
        VectorNormalize(dir);
        /* Windows retains the Z/Y/X dot product and reflection scale. Linux
         * accumulates X/Y/Z and rounds the scaled result before reflection. */
        reflectionScale = (((WIDE_FLOAT)dir[first] * trace.normal[first] + (WIDE_FLOAT)dir[1] * trace.normal[1])
                           + (WIDE_FLOAT)dir[last] * trace.normal[last]) * -2.0f;
        reflected[0] = (WIDE_FLOAT)trace.normal[0] * reflectionScale + dir[0];
        reflected[1] = (WIDE_FLOAT)trace.normal[1] * reflectionScale + dir[1];
        reflected[2] = (WIDE_FLOAT)trace.normal[2] * reflectionScale + dir[2];

        if ((trace.surfaceFlags & SURF_SKY) == 0 &&
            traceEnt->client == NULL) {
            if (hitEnt == attacker || trace.partGroup != 0) {
                gentity_t *event = G_TempEntity(trace.endpos, EV_BULLET_HIT);

                event->s.tempEffectId = DirToByte(trace.normal);
                event->s.hintStringIndex = DirToByte(reflected);
                event->s.surfType =
                    (trace.surfaceFlags &
                     (SURFACE_TYPE_MASK << SURFACE_TYPE_SHIFT)) >>
                    SURFACE_TYPE_SHIFT;
                event->s.weapon = weaponInfo->weaponIndex;
                event->s.vehicleEntityNum = source->s.number;
                if (attacker != NULL && attacker->client != NULL &&
                    (attacker->s.eFlags & EF_IN_VEHICLE) != 0) {
                    event->s.surfaceImpact.followClient =
                        coduo_int32_from_bits((uint32_t)attacker->passEntityNum +
                                              UINT32_C(1));
                    event->s.surfaceImpact.weaponAnim =
                        attacker->s.vehicleAnimState;
                }
                hit = qtrue;
            }
        } else if (hitEnt == attacker || traceEnt->client != NULL) {
            if (coduo_server_rand_unit() <
                game_compat_fire_weapon_tracer_chance(source)) {
                game_compat_fire_weapon_spawn_tracer(start, trace.endpos, weaponInfo);
            }
            hit = qtrue;
        }
    } else if (hitEnt == attacker) {
        if (coduo_server_rand_unit() <
            game_compat_fire_weapon_tracer_chance(source)) {
            game_compat_fire_weapon_spawn_tracer(start, end, weaponInfo);
        }
        hit = qtrue;
    }

    if ((trace.contents & CONTENTS_GLASS) != 0) {
        vec3_t penetrationDir;
        WIDE_FLOAT normalDot, step;

        game_compat_fire_weapon_vector_subtract(end, start, penetrationDir);
        VectorNormalize(penetrationDir);
        /* The Windows projection and quotient stay live. Linux stores the
         * dot product before negation and stores the quotient before use. */
        normalDot = ((WIDE_FLOAT)penetrationDir[first] * trace.normal[first] + (WIDE_FLOAT)penetrationDir[1] * trace.normal[1])
                    + (WIDE_FLOAT)penetrationDir[last] * trace.normal[last];
        normalDot = -normalDot;
        step = normalDot >= 0.125f ? 0.25f / normalDot : 0.0f;
        start[0] = (WIDE_FLOAT)penetrationDir[0] * step + trace.endpos[0];
        start[1] = (WIDE_FLOAT)penetrationDir[1] * step + trace.endpos[1];
        start[2] = (WIDE_FLOAT)penetrationDir[2] * step + trace.endpos[2];

        hit = (qboolean)(hit | Bullet_Fire_Extended(hitEnt, attacker, start, end,
                                                    damage,
                                                    coduo_int32_from_bits(
                                                        (uint32_t)recursionDepth +
                                                        UINT32_C(1)),
                                                    muzzle, source));
        return hit;
    }

    if (traceEnt->takeDamage == 0) {
        return hit;
    }

    damage = coduo_fp_to_i32_extended(
        Damage_Falloff(VectorDistance(start, trace.endpos),
                       (float)damage,
                       (float)weaponInfo->damageFalloffMinDamagePercent,
                       weaponInfo->damageFalloffMinRange,
                       weaponInfo->damageFalloffMaxRange));
    /* 0x7b125: bare fild of damage, no float32 rounding of the int. */
    WIDE_FLOAT penetrationDamage = damage;
    penetrationDamage = coduo_round_f32(penetrationDamage);
    if (damage <= 0.0f) {
        return hit;
    }

    damageInflictor = source != NULL ? source : attacker;
    if (damageInflictor == attacker && attacker != NULL && attacker->client != NULL &&
        (attacker->s.eFlags & EF_IN_VEHICLE) != 0 &&
        attacker->client->ps.vehiclePosition != 3) {
        damageInflictor = &g_entities[attacker->passEntityNum];
    }

    G_Damage(traceEnt, damageInflictor, attacker, muzzle->forward, trace.endpos, damage,
             damageFlags, meansOfDeath, trace.partGroup);

    /* Halving is exact, but its input differs by platform: Windows retains a
     * float conversion made before G_Damage; Linux converts the integer wide.
     * The positive half is in int32_t range before truncation. */
    if (traceEnt->client != NULL &&
        (damageFlags & DAMAGE_FLAG_RICOCHET) != 0 &&
        (WIDE_FLOAT)penetrationDamage * 0.5f > 0.0L) {
        hit = (qboolean)(hit | Bullet_Fire_Extended(traceEnt, attacker, trace.endpos, end,
                                                    (int32_t)((WIDE_FLOAT)penetrationDamage * 0.5f),
                                                    coduo_int32_from_bits(
                                                        (uint32_t)recursionDepth +
                                                        UINT32_C(1)),
                                                    muzzle,
                                                    source));
    }

    return hit;
}

/* ------------------------------------------------------------------ */
/*  0x7b2c4 weapon_grenadelauncher_fire                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7b2c4, 8b2c4_weapon_grenadelauncher_fire.c, VERIFY-FIRE-WEAPON-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
gentity_t *weapon_grenadelauncher_fire(gentity_t *ent, int weapon,
                                       weapon_muzzle_t *muzzle)
{
    const weaponInfo_t *weaponInfo = muzzle->weaponInfo;
    vec3_t dir;
    gentity_t *grenade;

    /* 0x7b2df-0x7b32b: Linux consumes integer speeds without a float integer-
     * conversion boundary and stores the Z product before the upward boost.
     * Windows keeps that product live. Native wide evaluation preserves the
     * integer inputs on non-x87 platforms without software emulation. */
    dir[0] = (WIDE_FLOAT)muzzle->forward[0] * weaponInfo->missileSpeed;
    dir[1] = (WIDE_FLOAT)muzzle->forward[1] * weaponInfo->missileSpeed;
    WIDE_FLOAT verticalProduct = (WIDE_FLOAT)muzzle->forward[2] * weaponInfo->missileSpeed;
    dir[2] = verticalProduct + weaponInfo->missileVerticalSpeed;

    grenade = fire_grenade(ent, muzzle->origin, dir, weapon);
    VectorNormalize(dir);
    game_compat_fire_weapon_add_projectile_inherited_velocity(grenade, ent, dir);
    return grenade;
}

/* ------------------------------------------------------------------ */
/*  0x7b3da Weapon_RocketLauncher_Fire                                */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7b3da, 8b3da_Weapon_RocketLauncher_Fire.c, VERIFY-FIRE-WEAPON-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void Weapon_RocketLauncher_Fire(gentity_t *ent, float spread,
                                weapon_muzzle_t *muzzle)
{
    vec3_t dir;
    vec3_t start;

    game_compat_fire_weapon_spread_direction(muzzle, spread, dir);
    game_compat_fire_weapon_vector_copy(muzzle->origin, start);
    fire_rocket(ent, start, dir);

    if (ent->client != NULL) {
        /* NO shim: -64.0f is a power of two so forward*-64 is exact in float
         * (exponent shift + sign); velocity + that exact value is a single add,
         * native-identical (rule 1/6). Same for the artillery copy below. */
        ent->client->ps.velocity[0] += muzzle->forward[0] * -64.0f;
        ent->client->ps.velocity[1] += muzzle->forward[1] * -64.0f;
        ent->client->ps.velocity[2] += muzzle->forward[2] * -64.0f;
    }
}

/* ------------------------------------------------------------------ */
/*  0x7b594 Weapon_Artillery_Fire                                     */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7b594, 8b594_Weapon_Artillery_Fire.c, VERIFY-FIRE-WEAPON-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void Weapon_Artillery_Fire(gentity_t *ent, float spread,
                           weapon_muzzle_t *muzzle)
{
    vec3_t dir;
    vec3_t start;
    gentity_t *rocket;

    game_compat_fire_weapon_spread_direction(muzzle, spread, dir);
    game_compat_fire_weapon_vector_copy(muzzle->origin, start);
    rocket = fire_rocket(ent, start, dir);
    rocket->s.pos.trType = TR_GRAVITY;

    if (ent->client != NULL) {
        ent->client->ps.velocity[0] += muzzle->forward[0] * -64.0f;
        ent->client->ps.velocity[1] += muzzle->forward[1] * -64.0f;
        ent->client->ps.velocity[2] += muzzle->forward[2] * -64.0f;
    }
}

/* ------------------------------------------------------------------ */
/*  0x7b758 Weapon_ArtilleryStrike_Fire                               */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7b758, 8b758_Weapon_ArtilleryStrike_Fire.c, VERIFY-FIRE-WEAPON-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void Weapon_ArtilleryStrike_Fire(gentity_t *ent, float spread,
                                 weapon_muzzle_t *muzzle)
{
    trace_t trace;
    vec3_t end;

    BG_Bullet_Endpos(spread, end, muzzle->forward);
    trap_LocationalTrace(&trace, muzzle->origin, end, ent->s.number,
                         MASK_BULLETTRACE, bulletPriorityMap);

    if (!(trace.fraction < 1.0f) ||
        (trace.surfaceFlags & SURF_SKY) != 0) {
        int weapon = BG_GetWeaponForInfo(muzzle->weaponInfo);
        Add_Ammo(ent, weapon, 1, qfalse);
        return;
    }

    end[0] = trace.endpos[0];
    end[1] = trace.endpos[1];
    end[2] = trace.endpos[2];
    fire_artillery_barrage(ent, end,
                           BG_GetWeaponForInfo(muzzle->weaponInfo));
}

/* ------------------------------------------------------------------ */
/*  0x7b841 LogAccuracyHit                                            */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7b841, 8b841_LogAccuracyHit.c, VERIFY-FIRE-WEAPON-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
int LogAccuracyHit(gentity_t *target, gentity_t *attacker)
{
    if (!game_compat_gentity_can_take_damage(target)) {
        return qfalse;
    }
    if (target == attacker) {
        return qfalse;
    }
    if (target->client == NULL || attacker->client == NULL) {
        return qfalse;
    }
    if (target->client->ps.pmType >= PM_TYPE_DEAD) {
        return qfalse;
    }
    return OnSameTeam(target, attacker) == 0;
}

/* ------------------------------------------------------------------ */
/*  0x7b8ea CalcMuzzlePoint                                           */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7b8ea, 8b8ea_CalcMuzzlePoint.c, VERIFY-FIRE-WEAPON-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void CalcMuzzlePoint(gentity_t *ent, float *muzzlePoint)
{
    gclient_t *client = ent->client;
    const vec3_t traceMins = { -8.0f, -8.0f, -8.0f };
    const vec3_t traceMaxs = { 8.0f, 8.0f, 8.0f };

    muzzlePoint[0] = client->ps.psOrigin[0];
    muzzlePoint[1] = client->ps.psOrigin[1];
    muzzlePoint[2] = client->ps.psOrigin[2];

    if ((client->ps.entityStateFlags & EF_IN_VEHICLE) == 0) {
        muzzlePoint[2] += client->ps.viewHeightCurrent;
    } else {
        muzzlePoint[2] += 28.0f;
    }

    if ((client->ps.entityStateFlags & EF_FORCED_STANCE_MASK) == 0) {
        const weaponInfo_t *weaponInfo =
            (const weaponInfo_t *)BG_GetInfoForWeapon(ent->s.weapon);
        float bobCycle;
        float speed;

        if (weaponInfo->weaponClass == WEAPCLASS_LMG &&
            (client->ps.playerStateFlags & PMF_ADS) != 0) {
            vec3_t angles = { 0.0f, client->ps.proneDirection, 0.0f };
            vec3_t forward;
            vec3_t end;
            trace_t trace;

            AngleVectors(angles, forward, NULL, NULL);
            /* Keep the forward offset wide until each endpoint store. */
            for (int i = 0; i < 3; i++) {
                end[i] = (float)((long double)forward[i] * MUZZLE_ADS_FORWARD_DISTANCE + muzzlePoint[i]);
            }
            trap_Trace(&trace, muzzlePoint, traceMins, traceMaxs, end, ent->s.number,
                       MASK_GRENADE_TRACE);
            /* A trace that exits solid still supplies a usable endpoint. */
            if (trace.allsolid == 0) {
                game_compat_fire_weapon_vector_copy(trace.endpos, muzzlePoint);
            }
        }

        /* This caller inlines division by 255 before scaling the phase;
         * the out-of-line BG_GetBobCycle uses a rounded reciprocal instead. */
        bobCycle = (float)(((long double)(uint8_t)client->ps.bobCycle / MUZZLE_BOB_CYCLE_DIVISOR) * MUZZLE_BOB_PHASE_TWO_PI + MUZZLE_BOB_PHASE_TWO_PI);
        speed = BG_GetSpeed(&client->ps, level.time);

        /* Both original callers store the vertical bob as binary32 before
         * adding it to the muzzle height. */
        float verticalBob = (float)BG_GetVerticalBobFactor(&client->ps, bobCycle, speed, bg_bobMax.value);
        muzzlePoint[2] += verticalBob;
        {
            vec3_t right;
            /* The inlined Windows path rounds both the scaled amplitude and
             * sine before multiplying them, unlike the out-of-line helper. */
            float stanceScale;
            if (client->ps.viewHeightTarget == client->ps.proneViewHeight) {
                stanceScale = bg_bobAmplitudeProne.value;
            } else if (client->ps.viewHeightTarget == client->ps.crouchViewHeight) {
                stanceScale = bg_bobAmplitudeDucked.value;
            } else {
                stanceScale = bg_bobAmplitudeStanding.value;
            }
            float amplitude = stanceScale * speed;
            if (amplitude > bg_bobMax.value) {
                amplitude = bg_bobMax.value;
            }
            float sine = (float)coduomp_crt_sinl((WIDE_FLOAT)bobCycle);
            float bob = sine * amplitude;

            AngleVectors(client->ps.viewAngles, NULL, right, NULL);
            muzzlePoint[0] = (float)((long double)right[0] * bob + muzzlePoint[0]);
            muzzlePoint[1] = (float)((long double)right[1] * bob + muzzlePoint[1]);
            muzzlePoint[2] = (float)((long double)right[2] * bob + muzzlePoint[2]);
        }

        G_AddLean(ent, muzzlePoint);

        /* Windows stores origin.z + 8 as binary32 before comparing it.
         * Linux keeps the sum wide through the comparison and only narrows
         * the replacement height. */
        float minimumViewZ = client->ps.psOrigin[2] + MUZZLE_MIN_VIEW_HEIGHT;
        if (muzzlePoint[2] < minimumViewZ) {
            muzzlePoint[2] = minimumViewZ;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x7bc58 CalcMuzzlePoints                                          */
/* ------------------------------------------------------------------ */

/* VERIFIED_DECOMPILER(0x7bc58, 8bc58_CalcMuzzlePoints.c, VERIFY-FIRE-WEAPON-REMAINING-2026-06-17): DATAFLOW_VERIFIED */
void CalcMuzzlePoints(gentity_t *ent, weapon_muzzle_t *muzzle)
{
    vec3_t angles;

    angles[0] = ent->client->spectatorSnapshotAngle0;
    angles[1] = ent->client->spectatorSnapshotAngle1;
    angles[2] = ent->client->ps.viewAngles[2];

    AngleVectors(angles, muzzle->forward, muzzle->right, muzzle->up);
    CalcMuzzlePoint(ent, muzzle->origin);
}

/* ------------------------------------------------------------------ */
/*  0x7bd05  FireWeapon                                               */
/* ------------------------------------------------------------------ */

/*
 * Fire the player's current weapon.
 *
 * This function handles weapon firing for all weapon types:
 *  1. Checks if weapon can be fired (vehicle position, etc.)
 *  2. Gets weapon info and calculates muzzle points
 *  3. Calculates spread based on weapon and player state
 *  4. Dispatches to appropriate firing function based on weapon type:
 *     - Type 0: Bullet_Fire (hitscan weapons)
 *     - Type 1: weapon_grenadelauncher_fire (grenades)
 *     - Type 2: Weapon_RocketLauncher_Fire (projectiles)
 *     - Type 3: Weapon_ArtilleryStrike_Fire (spotter/artillery)
 */
/* VERIFIED_DECOMPILER(0x7bd05, 8bd05_FireWeapon.c, VERIFY-FIRE-WEAPON-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void FireWeapon(gentity_t *ent)
{
    weapon_muzzle_t muzzlePoints;
    long double minSpread;
    float maxSpread;
    float spread;
    float spreadFraction;
    int weaponType;
    int weapon;

    /* Check if weapon can be fired. */
    if (((ent->client->ps.entityStateFlags & EF_RESTRICTED_MASK) == 0) ||
        (ent->activeState == 0) ||
        (BG_AllowPlayerWeaponAtVehiclePos(ent->client->ps.vehicleType,
                                          ent->client->ps.vehiclePosition) != 0)) {

        /* Get weapon info */
        weapon = ent->s.weapon;
        const weaponInfo_t *wInfo = (const weaponInfo_t *)BG_GetInfoForWeapon(weapon);

        /* Calculate muzzle points */
        muzzlePoints.weaponInfo = wInfo;
        CalcMuzzlePoints(ent, &muzzlePoints);

        /* Get the normalized spread accumulator for the shot calculation. */
        spreadFraction = ent->client->damageAlphaFraction;

        /* Calculate spread */
        minSpread = BG_GetMinSpreadForWeapon(&ent->client->ps, ent->s.weapon,
                                             level.time,
                                             ent->client->ps.adsFraction == 1.0f);

        maxSpread = wInfo->maxSpread;

        /* Interpolate from the stance minimum using the spread accumulator.
         * Windows retains the minimum's live return through this expression;
         * Linux first stores the minimum as binary32. */
        spread = (float)(((long double)maxSpread - minSpread) * spreadFraction + minSpread);

        /* Get weapon type */
        weaponType = wInfo->weaponType;

        /* Dispatch to appropriate firing function */
        if (weaponType == WEAPTYPE_BULLET) {
            Bullet_Fire(ent, spread, wInfo->flameDamage,
                       &muzzlePoints, ent);
        } else if (weaponType == WEAPTYPE_GRENADE) {
            weapon_grenadelauncher_fire(ent, ent->s.weapon,
                                        &muzzlePoints);
        } else if (weaponType == WEAPTYPE_PROJECTILE) {
            Weapon_RocketLauncher_Fire(ent, spread, &muzzlePoints);
        } else if (weaponType == WEAPTYPE_SPOTTER) {
            Weapon_ArtilleryStrike_Fire(ent, spread, &muzzlePoints);
        } else if (weaponType != WEAPTYPE_GAS) {
            G_Error("Unknown weapon type %i for %s\n", weaponType,
                   wInfo->pickupName);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  0x7bef7  FireWeaponMelee                                          */
/* ------------------------------------------------------------------ */

/*
 * Fire melee weapon attack.
 *
 * This function handles melee weapon attacks:
 *  1. Checks whether the fire position permits melee.
 *  2. Gets weapon info for the event weapon.
 *  3. Calculates forward/right/up from view angles.
 *  4. Calculates the muzzle point.
 *  5. Calls Weapon_Melee to perform the melee trace and damage.
 */
/* VERIFIED_DECOMPILER(0x7bef7, 8bef7_FireWeaponMelee.c, VERIFY-FIRE-WEAPON-PACKET-2026-06-17): DATAFLOW_VERIFIED */
void FireWeaponMelee(gentity_t *ent)
{
    gclient_t *client = ent->client;
    float muzzlePoints[12];

    if (((client->ps.entityStateFlags & EF_RESTRICTED_MASK) == 0) ||
        (ent->activeState == 0)) {

        (void)BG_GetInfoForWeapon(ent->s.weapon);

        AngleVectors(client->ps.viewAngles, muzzlePoints, &muzzlePoints[3],
                     &muzzlePoints[6]);
        CalcMuzzlePoint(ent, &muzzlePoints[9]);
        Weapon_Melee(ent, muzzlePoints);
    }
}
