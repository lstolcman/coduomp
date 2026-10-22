#include "fx_classes.hpp"
#include "fx_archive.h"

#include "../math/vector_math.h"
#include "../physics/cm_trace.h"
#include "../platform/crt_boundary.h"

#include <string.h>

/* Source: CoDUOMP.exe 0x004a10c0..0x004a10d7.
 * Class binding: Windows vtable 0x005a1fc4; same-module Mac CParticle
 * virtual-method set. */
CParticle::CParticle()
{
    renderEntity.reType = RT_SPRITE;
}

/* Source: CoDUOMP.exe 0x004a1110..0x004a1125.
 * Name: same-module Mac symbol CParticle::~CParticle. */
CParticle::~CParticle() = default;

/* Source: CoDUOMP.exe 0x004aeef0..0x004aef23.
 * The Windows LTCG calling convention carries this in EAX and value in ECX.
 * The Mac compiler inlined this operation, so the method name is derived from
 * the proved destination field at CParticle+0xe0. */
void CParticle::SetVelocity(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&particleVelocity[0], &value[0],
               sizeof(particleVelocity[0]));
        memcpy(&particleVelocity[1], &value[1],
               sizeof(particleVelocity[1]));
        memcpy(&particleVelocity[2], &value[2],
               sizeof(particleVelocity[2]));
    } else {
        particleVelocity[2] = 0.0f;
        particleVelocity[1] = 0.0f;
        particleVelocity[0] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004aef30..0x004aef63.
 * Name is derived from the proved CParticle+0xec destination field. */
void CParticle::SetVelocityGoal(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&velocityGoal[0], &value[0], sizeof(velocityGoal[0]));
        memcpy(&velocityGoal[1], &value[1], sizeof(velocityGoal[1]));
        memcpy(&velocityGoal[2], &value[2], sizeof(velocityGoal[2]));
    } else {
        velocityGoal[2] = 0.0f;
        velocityGoal[1] = 0.0f;
        velocityGoal[0] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004aef70..0x004aefa3.
 * Name is derived from the proved CParticle+0xf8 destination field. */
void CParticle::SetAcceleration(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&acceleration[0], &value[0], sizeof(acceleration[0]));
        memcpy(&acceleration[1], &value[1], sizeof(acceleration[1]));
        memcpy(&acceleration[2], &value[2], sizeof(acceleration[2]));
    } else {
        acceleration[2] = 0.0f;
        acceleration[1] = 0.0f;
        acceleration[0] = 0.0f;
    }
}

/* Source: CoDUOMP.exe 0x004aefb0..0x004af11c.
 * These Windows out-of-line copies are the particle channel setters. The Mac
 * compiler inlined them, so their role names follow the proved CParticle
 * fields and the FX_Add* factory arguments. */
void CParticle::SetSizeStart(float value)
{
    memcpy(&sizeStart, &value, sizeof(sizeStart));
}

void CParticle::SetSizeEnd(float value)
{
    memcpy(&sizeEnd, &value, sizeof(sizeEnd));
}

void CParticle::SetSizeTime(float value)
{
    memcpy(&sizeTime, &value, sizeof(sizeTime));
}

void CParticle::SetSize2Start(float value)
{
    memcpy(&size2Start, &value, sizeof(size2Start));
}

void CParticle::SetSize2End(float value)
{
    memcpy(&size2End, &value, sizeof(size2End));
}

void CParticle::SetSize2Time(float value)
{
    memcpy(&size2Time, &value, sizeof(size2Time));
}

void CParticle::SetColorStart(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&colorStart[0], &value[0], sizeof(colorStart[0]));
        memcpy(&colorStart[1], &value[1], sizeof(colorStart[1]));
        memcpy(&colorStart[2], &value[2], sizeof(colorStart[2]));
    } else {
        colorStart[2] = 0.0f;
        colorStart[1] = 0.0f;
        colorStart[0] = 0.0f;
    }
}

void CParticle::SetColorEnd(const vec3_t value)
{
    if (value != nullptr) {
        memcpy(&colorEnd[0], &value[0], sizeof(colorEnd[0]));
        memcpy(&colorEnd[1], &value[1], sizeof(colorEnd[1]));
        memcpy(&colorEnd[2], &value[2], sizeof(colorEnd[2]));
    } else {
        colorEnd[2] = 0.0f;
        colorEnd[1] = 0.0f;
        colorEnd[0] = 0.0f;
    }
}

void CParticle::SetColorTime(float value)
{
    memcpy(&colorTime, &value, sizeof(colorTime));
}

void CParticle::SetAlphaStart(float value)
{
    memcpy(&alphaStart, &value, sizeof(alphaStart));
}

void CParticle::SetAlphaEnd(float value)
{
    memcpy(&alphaEnd, &value, sizeof(alphaEnd));
}

void CParticle::SetAlphaTime(float value)
{
    memcpy(&alphaTime, &value, sizeof(alphaTime));
}

/* Source: CoDUOMP.exe 0x004af0d0..0x004af0dc.
 * This retained particle-channel setter writes the refEntity rotation lane at
 * CParticle+0xc0. CParticle::UpdateRotation consumes it as float; the Mac
 * compiler inlined the corresponding factory assignments. */
void CParticle::SetRotation(float value)
{
    memcpy(&renderEntity.rotation, &value, sizeof(renderEntity.rotation));
}

void CParticle::SetRotationGoal(float value)
{
    memcpy(&rotationGoal, &value, sizeof(rotationGoal));
}

void CParticle::SetRotationVelocity(float value)
{
    memcpy(&rotationVelocity, &value, sizeof(rotationVelocity));
}

void CParticle::SetRotationAcceleration(float value)
{
    memcpy(&rotationAcceleration, &value, sizeof(rotationAcceleration));
}

void CParticle::SetElasticity(float value)
{
    memcpy(&elasticity, &value, sizeof(elasticity));
}

/* Source: CoDUOMP.exe 0x004aeee0..0x004aeee6.
 * This retained accessor immediately precedes the CParticle channel-setter
 * bank and writes the proved sprite shader slot at CEffect+0xa8. The Mac
 * compiler inlined the operation, so the role name is derived from the
 * destination field. */
void CParticle::SetSpriteShaderHandle(int32_t value)
{
    renderEntity.spriteShaderHandle = value;
}

/* Source: CoDUOMP.exe 0x004a1130..0x004a1203.
 * Name: same-module Mac symbol CParticle::Die. */
void CParticle::Die()
{
    if ((flags & FX_EFFECT_FLAG_PLAY_DEATH_EFFECT) == 0U ||
        (flags & FX_EFFECT_FLAG_DIE_ON_IMPACT) != 0U) {
        return;
    }

    vec3_t direction = {
        particleVelocity[0], particleVelocity[1], particleVelocity[2]
    };
    if (VectorNormalize(direction) < 0.10000000149011612f) {
        for (int component = 0; component < 3; ++component) {
            direction[component] =
                static_cast<float>(coduo_crt_rand()) *
                    3.0517578125e-05f * 2.0f -
                1.0f;
        }
        (void)VectorNormalize(direction);
    }

    CFxScheduler_PlayEffectID(&fxScheduler, deathResource.effectId,
                              origin, direction);
}

/* Source: CoDUOMP.exe 0x004a12e0..0x004a16fe.
 * Name: same-module Mac symbol CParticle::UpdateOrigin. */
qboolean CParticle::UpdateOrigin()
{
    enum {
        FX_TRACE_PASS_ENTITY_NUM = 0,
        FX_TRACE_CONTENT_MASK = CONTENTS_SOLID
    };
    constexpr float millisecondsToSeconds = 0.0010000000474974513f;
    constexpr float reflectionMultiplier = -2.0f;
    constexpr float settledVerticalSpeed = 4.0f;
    UpdateVelocity();

    /* The time square uses one live product and one binary32 spill. */
    const long double liveFrameSeconds = static_cast<long double>(fxFrameTime) * millisecondsToSeconds;
    const float frameSeconds = static_cast<float>(liveFrameSeconds);
    const long double halfFrameSecondsSquared = liveFrameSeconds * frameSeconds * 0.5f;
    vec3_t nextOrigin;

    /* 0x004a130e..0x004a1325 sums the two motion products before adding the
     * origin component last. */
    for (int component = 0; component < 3; ++component) {
        nextOrigin[component] = static_cast<float>(
            halfFrameSecondsSquared * acceleration[component] +
            static_cast<long double>(frameSeconds) * particleVelocity[component] +
            origin[component]);
    }

    if ((flags & FX_EFFECT_FLAG_TRACE_COLLISION) == 0U) {
        for (int component = 0; component < 3; ++component) {
            origin[component] = nextOrigin[component];
        }
        return qtrue;
    }

    vec3_t traceStart;
    vec3_t traceEnd;
    orientation_t *boltOrientation = nullptr;
    if (boltFrame.frame != nullptr) {
        boltOrientation = CFxBoltFrame_GetOrientation(boltFrame.frame);
        if (boltOrientation == nullptr) {
            return qfalse;
        }
        /* These inlined transforms retain different addition orders for
         * the start and end Y/Z lanes. Each lane rounds only at its store. */
        for (int component = 0; component < 3; ++component) {
            const long double startX = static_cast<long double>(origin[0]) * boltOrientation->axis[0][component];
            const long double startY = static_cast<long double>(origin[1]) * boltOrientation->axis[1][component];
            const long double startZ = static_cast<long double>(origin[2]) * boltOrientation->axis[2][component];
            const long double endX = static_cast<long double>(nextOrigin[0]) * boltOrientation->axis[0][component];
            const long double endY = static_cast<long double>(nextOrigin[1]) * boltOrientation->axis[1][component];
            const long double endZ = static_cast<long double>(nextOrigin[2]) * boltOrientation->axis[2][component];
            traceStart[component] = static_cast<float>(
                (component == 0 ? (startZ + startX) + startY : (startZ + startY) + startX) +
                boltOrientation->origin[component]);
            traceEnd[component] = static_cast<float>(
                (component == 0 ? (endZ + endX) + endY : (endX + endZ) + endY) +
                boltOrientation->origin[component]);
        }
    } else {
        for (int component = 0; component < 3; ++component) {
            traceStart[component] = origin[component];
            traceEnd[component] = nextOrigin[component];
        }
    }

    const float *mins = nullptr;
    const float *maxs = nullptr;
    if ((flags & FX_EFFECT_FLAG_TRACE_VOLUME) != 0U) {
        mins = traceMins;
        maxs = traceMaxs;
    }

    trace_t trace;
    /* 0x004a14a9..0x004a14de: the two zero registers are the absent
     * mins/maxs for a point trace; the volume path supplies traceMins and
     * traceMaxs. Both paths call the collision-model trace with model 0,
     * CONTENTS_SOLID, and capsule=false through SFxHelper::Trace. */
    SFxHelper_Trace(&trace, traceStart, mins, maxs, traceEnd,
                    FX_TRACE_PASS_ENTITY_NUM, FX_TRACE_CONTENT_MASK);

    if ((trace.surfaceFlags & SURF_SKY) != 0 ||
        !(trace.fraction < 1.0f) || trace.startsolid != 0U ||
        trace.allsolid != 0U) {
        for (int component = 0; component < 3; ++component) {
            origin[component] = nextOrigin[component];
        }
        return qtrue;
    }

    if ((flags & FX_EFFECT_FLAG_PLAY_IMPACT_EFFECT) != 0U) {
        CFxScheduler_PlayEffectID(&fxScheduler,
                                  impactResource.effectId,
                                  trace.endpos, trace.normal);
    }
    const uint32_t impactFlags = flags;
    if ((impactFlags & FX_EFFECT_FLAG_DIE_ON_IMPACT) != 0U) {
        return qfalse;
    }

    vec3_t collisionNormal;
    if (boltFrame.frame != nullptr) {
        OrientationDirFromWorldDir(boltOrientation, trace.normal,
                                   collisionNormal);
    } else {
        for (int component = 0; component < 3; ++component) {
            collisionNormal[component] = trace.normal[component];
        }
    }

    const long double impactSeconds = static_cast<long double>(trace.fraction) * frameSeconds;
    const long double impactVelocityX = impactSeconds * acceleration[0] + particleVelocity[0];
    particleVelocity[0] = static_cast<float>(impactVelocityX);
    const float impactVelocityY = static_cast<float>(impactSeconds * acceleration[1] + particleVelocity[1]);
    const float impactVelocityZ = static_cast<float>(impactSeconds * acceleration[2] + particleVelocity[2]);
    particleVelocity[1] = impactVelocityY;
    particleVelocity[2] = impactVelocityZ;

    /* X keeps its live velocity and reflection factor; Y/Z use the float
     * spills. The settling comparison consumes live Z after its store. */
    const long double normalVelocity =
        (static_cast<long double>(impactVelocityZ) * collisionNormal[2] +
         static_cast<long double>(impactVelocityY) * collisionNormal[1]) +
        impactVelocityX * collisionNormal[0];
    const long double liveReflectionScale = normalVelocity * reflectionMultiplier;
    const float reflectionScale = static_cast<float>(liveReflectionScale);
    particleVelocity[0] = static_cast<float>((liveReflectionScale * collisionNormal[0] + impactVelocityX) * elasticity);
    particleVelocity[1] = static_cast<float>((static_cast<long double>(reflectionScale) * collisionNormal[1] + impactVelocityY) * elasticity);
    const long double bounceVelocityZ = (static_cast<long double>(reflectionScale) * collisionNormal[2] + impactVelocityZ) * elasticity;
    particleVelocity[2] = static_cast<float>(bounceVelocityZ);

    if (trace.normal[2] > 0.0f && bounceVelocityZ < settledVerticalSpeed) {
        for (int component = 2; component >= 0; --component) {
            particleVelocity[component] = 0.0f;
        }
        for (int component = 2; component >= 0; --component) {
            acceleration[component] = 0.0f;
        }
        flags = impactFlags & ~(FX_EFFECT_FLAG_TRACE_COLLISION | FX_EFFECT_FLAG_PLAY_IMPACT_EFFECT);
    }

    for (int component = 0; component < 3; ++component) {
        origin[component] = static_cast<float>(
            (static_cast<long double>(nextOrigin[component]) - origin[component]) * trace.fraction + origin[component]);
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a1250..0x004a12d2.
 * Name: same-module Mac symbol CParticle::Update. */
qboolean CParticle::Update()
{
    if (timeStart > fxCurrentTime || UpdateOrigin() == qfalse) {
        return qfalse;
    }

    if (boltFrame.frame != nullptr) {
        orientation_t *orientation =
            CFxBoltFrame_GetOrientation(boltFrame.frame);
        if (orientation == nullptr) {
            return qfalse;
        }
        OrientationPosToWorldPos(orientation, origin, renderEntity.origin);
    } else {
        for (int component = 0; component < 3; ++component) {
            renderEntity.origin[component] = origin[component];
        }
    }

    UpdateSize();
    UpdateSize2();
    UpdateRGB();
    UpdateAlpha();
    UpdateRotation();
    return qtrue;
}

/* Source: CoDUOMP.exe 0x004a21c0..0x004a21c2. */
int32_t CParticle::TypeID()
{
    return FX_EFFECT_TYPE_PARTICLE;
}

/* Source: CoDUOMP.exe 0x004a1210..0x004a1224.
 * Name: same-module Mac symbol CParticle::Cull. */
qboolean CParticle::Cull()
{
    return SFxHelper_CullSphere(renderEntity.origin, renderEntity.radius);
}

/* Source: CoDUOMP.exe 0x004a1230..0x004a124c.
 * Name: same-module Mac symbol CParticle::Draw. */
void CParticle::Draw()
{
    if ((flags & FX_EFFECT_FLAG_DEPTH_HACK) != 0U) {
        renderEntity.renderfx |= FX_RENDER_FLAG_DEPTH_HACK;
    }
    SFxHelper_AddFxToScene(&renderEntity);
}

/* Source: CoDUOMP.exe 0x004a21d0..0x004a2430.
 * Name: same-module Mac symbol CParticle::Archive. */
void CParticle::Archive(fx_archive_t &archive)
{
    CEffect::Archive(archive);
    CFxArchive_ArchiveVec3(&archive, particleVelocity);
    CFxArchive_ArchiveVec3(&archive, velocityGoal);
    CFxArchive_ArchiveVec3(&archive, acceleration);
    CFxArchive_ArchiveFloat(&archive, &sizeStart);
    CFxArchive_ArchiveFloat(&archive, &sizeEnd);
    CFxArchive_ArchiveFloat(&archive, &sizeTime);
    CFxArchive_ArchiveFloat(&archive, &size2Start);
    CFxArchive_ArchiveFloat(&archive, &size2End);
    CFxArchive_ArchiveFloat(&archive, &size2Time);
    CFxArchive_ArchiveVec3(&archive, colorStart);
    CFxArchive_ArchiveVec3(&archive, colorEnd);
    CFxArchive_ArchiveFloat(&archive, &colorTime);
    CFxArchive_ArchiveFloat(&archive, &alphaStart);
    CFxArchive_ArchiveFloat(&archive, &alphaEnd);
    CFxArchive_ArchiveFloat(&archive, &alphaTime);
    CFxArchive_ArchiveFloat(&archive, &rotationGoal);
    CFxArchive_ArchiveFloat(&archive, &rotationVelocity);
    CFxArchive_ArchiveFloat(&archive, &rotationAcceleration);
    CFxArchive_ArchiveFloat(&archive, &elasticity);
}
