/*
 * Source reconstruction for client transform and userinfo-name helpers.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_x87_eval.h"
#include "recovered_game.h"
#include "game_globals.h"
#include "game_functions.h"

#define CLIENT_ORIGIN_Z_SNAP_OFFSET 1.0f
#define CLIENT_ORIGIN_TELEPORT_BIT 0x00000008u
#define CLIENT_ANGLE_SHORT_SCALE 0x1.6c16c2p+7f /* original float32 0x43360b61 */
#define CLIENT_ANGLE_SHORT_TO_DEGREES 0.0054931640625f
#define CLIENT_PRONE_YAW_LIMIT_SHORT 8192.0f
#define CLIENT_PRONE_YAW_LIMIT 45.0f
#define CLIENT_PRONE_PITCH_UP_LIMIT 45.0f
#define CLIENT_PRONE_PITCH_DOWN_LIMIT -15.0f
#define CLIENT_PRONE_PITCH_DOWN_ADJUST 15.0f
#define CLIENT_NAME_DEFAULT "UnnamedPlayer"
#define CLIENT_BADINFO "\\name\\badinfo"
#define CLIENT_USERINFO_BUFFER_SIZE 1024
#define CLIENT_NAME_SIZE 32
char ColorIndex(char value);
void trap_GetUserinfo(int clientNum, char *buffer, int bufferSize);
int trap_IsLocalClient(int clientNum);

/* NOT_FROM_ORIGINAL_SOURCE: helper for repeated x87 truncate angle conversion. */
static int32_t game_compat_client_transform_angle_to_short(float angle)
{
    /* SetClientViewAngle 0x44402..0x445df keeps each product live through the
     * truncating fistp; this helper is the native-width adaptation site.
     * Windows converts through signed qword before taking the low 16 bits;
     * Linux uses signed dword conversion. */
    uint32_t packed = coduo_fp_to_u32_extended(
        (WIDE_FLOAT)angle * (WIDE_FLOAT)CLIENT_ANGLE_SHORT_SCALE);
    return (int32_t)(packed & 0xffffu);
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for the SetClientViewAngle delta-angle loop. */
static void game_compat_client_transform_update_delta_angles(gclient_t *client,
                                              const vec3_t angles)
{
    for (int axis = 0; axis < 3; axis++) {
        client->ps.deltaAngles[axis] = coduo_int32_from_bits(
            (uint32_t)game_compat_client_transform_angle_to_short(angles[axis]) -
            (uint32_t)client->command.angles[axis]);
    }
}

/* NOT_FROM_ORIGINAL_SOURCE: helper for SetClientViewAngle prone clamp branches. */
static void game_compat_client_transform_clamp_prone_view(gclient_t *client, vec3_t angles)
{
    if ((client->ps.playerStateFlags & PMF_PRONE) == 0 ||
        (client->ps.entityStateFlags & EF_RESTRICTED_MASK) != 0) {
        return;
    }

    float proneYaw = client->ps.proneDirection;
    float yawDelta = AngleNormalize180(AngleDelta(proneYaw, angles[1]));

    if (yawDelta > CLIENT_PRONE_YAW_LIMIT ||
        yawDelta < -CLIENT_PRONE_YAW_LIMIT) {
        if (yawDelta > CLIENT_PRONE_YAW_LIMIT) {
            yawDelta -= CLIENT_PRONE_YAW_LIMIT;
        } else {
            yawDelta += CLIENT_PRONE_YAW_LIMIT;
        }

        client->ps.deltaAngles[1] = coduo_int32_from_bits(
            (uint32_t)client->ps.deltaAngles[1] +
            (uint32_t)game_compat_client_transform_angle_to_short(yawDelta));

        /* Windows retains the subtraction through packing. Its other branch
         * multiplies first, then adds the folded short-angle offset. Linux
         * rounds the adjusted angle to float before the common packing step. */
        WIDE_FLOAT adjustedYaw = yawDelta > 0.0f ?
            (WIDE_FLOAT)proneYaw - CLIENT_PRONE_YAW_LIMIT :
            (WIDE_FLOAT)proneYaw + CLIENT_PRONE_YAW_LIMIT;
        WIDE_FLOAT packedYaw = adjustedYaw * CLIENT_ANGLE_SHORT_SCALE;
        if (!(yawDelta > 0.0f)) {
            packedYaw = (WIDE_FLOAT)proneYaw * CLIENT_ANGLE_SHORT_SCALE + CLIENT_PRONE_YAW_LIMIT_SHORT;
        }
        angles[1] = (float)(coduo_fp_to_u32_extended(packedYaw) & 0xffffu) * CLIENT_ANGLE_SHORT_TO_DEGREES;
    }

    float pronePitch = client->ps.proneTorsoPitch;
    float pitchDelta = AngleNormalize180(AngleDelta(pronePitch, angles[0]));

    if (pitchDelta > CLIENT_PRONE_PITCH_UP_LIMIT ||
        pitchDelta < CLIENT_PRONE_PITCH_DOWN_LIMIT) {
        if (pitchDelta > CLIENT_PRONE_PITCH_UP_LIMIT) {
            pitchDelta -= CLIENT_PRONE_PITCH_UP_LIMIT;
        } else {
            pitchDelta += CLIENT_PRONE_PITCH_DOWN_ADJUST;
        }

        client->ps.deltaAngles[0] = coduo_int32_from_bits(
            (uint32_t)client->ps.deltaAngles[0] +
            (uint32_t)game_compat_client_transform_angle_to_short(pitchDelta));

        if (pitchDelta > 0.0f) {
            angles[0] =
                AngleNormalize180(pronePitch - CLIENT_PRONE_PITCH_UP_LIMIT);
        } else {
            angles[0] =
                AngleNormalize180(pronePitch + CLIENT_PRONE_PITCH_DOWN_ADJUST);
        }
    }
}

/* VERIFIED_DECOMPILER(0x44210, 54210_SetClientOrigin.c, VERIFY-WAVE4-CLIENT-TRANSFORM-2026-06-17): DATAFLOW_VERIFIED - psOrigin copy, +1.0 Z snap, entityStateFlags bit-8 toggle, BG_PlayerStateToEntityState call, and currentOrigin mirror checked. */
void SetClientOrigin(gentity_t *ent, const float *origin)
{
    gclient_t *client = ent->client;

    client->ps.psOrigin[0] = origin[0];
    client->ps.psOrigin[1] = origin[1];
    client->ps.psOrigin[2] = origin[2] + CLIENT_ORIGIN_Z_SNAP_OFFSET;
    client->ps.entityStateFlags ^= CLIENT_ORIGIN_TELEPORT_BIT;

    BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);

    /* 0x442bc..0x442f5 reloads ent->client after the export call. */
    ent->currentOrigin[0] = ent->client->ps.psOrigin[0];
    ent->currentOrigin[1] = ent->client->ps.psOrigin[1];
    ent->currentOrigin[2] = ent->client->ps.psOrigin[2];
}

/* VERIFIED_DECOMPILER(0x44301, 54301_SetClientViewAngle.c, VERIFY-WAVE3-CLIENT-TRANSFORM-VIEW-2026-06-17): DATAFLOW_VERIFIED - input angle copy, prone yaw/pitch clamp helpers, delta-angle stores, currentAngles mirror, and client viewAngles mirror checked against current decompiler output. */
void SetClientViewAngle(gentity_t *ent, const float *angle)
{
    gclient_t *client = ent->client;
    vec3_t angles;

    angles[0] = angle[0];
    angles[1] = angle[1];
    angles[2] = angle[2];

    game_compat_client_transform_clamp_prone_view(client, angles);
    game_compat_client_transform_update_delta_angles(client, angles);

    ent->currentAngles[0] = angles[0];
    ent->currentAngles[1] = angles[1];
    ent->currentAngles[2] = angles[2];
    client->ps.viewAngles[0] = ent->currentAngles[0];
    client->ps.viewAngles[1] = ent->currentAngles[1];
    client->ps.viewAngles[2] = ent->currentAngles[2];
}

/* VERIFIED_DECOMPILER(0x44692, 54692_FUN_00054692.c, VERIFY-WAVE4-CLIENT-TRANSFORM-2026-06-17): DATAFLOW_VERIFIED - leading-space skip, color escape handling, visible-character count, space run limit, buffer gates, terminator, and fallback copy checked. */
void ClientCleanName(const char *source, char *dest, int destSize)
{
    char *start = dest;
    int written = 0;
    int visibleChars = 0;
    int consecutiveSpaces = 0;

    *dest = '\0';

    while (*source != '\0') {
        char ch = *source++;

        if (*start == '\0' && ch == ' ') {
            continue;
        }

        if (ch == '^') {
            if (*source == '\0') {
                break;
            }

            if (ColorIndex((unsigned char)*source) == 0) {
                source++;
                continue;
            }

            /* VERIFIED_DECOMPILER(0x44692, 54692_FUN_00054692.c, VERIFY-WAVE4-CLIENT-TRANSFORM-2026-06-17): DATAFLOW_VERIFIED - two-byte color-code capacity gate. */
            if (destSize - 3 < written) {
                break;
            }

            *dest++ = ch;
            *dest++ = *source++;
            written += 2;
            continue;
        }

        if (ch == ' ') {
            consecutiveSpaces++;
            if (consecutiveSpaces > 3) {
                continue;
            }
        } else {
            consecutiveSpaces = 0;
        }

        /* VERIFIED_DECOMPILER(0x44692, 54692_FUN_00054692.c, VERIFY-WAVE4-CLIENT-TRANSFORM-2026-06-17): DATAFLOW_VERIFIED - one-byte character capacity gate. */
        if (destSize - 2 < written) {
            break;
        }

        *dest++ = ch;
        visibleChars++;
        written++;
    }

    *dest = '\0';

    if (*start == '\0' || visibleChars == 0) {
        /* VERIFIED_DECOMPILER(0x44692, 54692_FUN_00054692.c, VERIFY-WAVE4-CLIENT-TRANSFORM-2026-06-17): DATAFLOW_VERIFIED - fallback copy uses destSize - 1. */
        Q_strncpyz(start, CLIENT_NAME_DEFAULT, destSize - 1);
    }
}

/* VERIFIED_DECOMPILER(0x447d5, 547d5_ClientUserinfoChanged.c, VERIFY-WAVE4-CLIENT-TRANSFORM-2026-06-17): DATAFLOW_VERIFIED - userinfo read/validation, local-client and predictItems stores, name-clean branches, handicap clamp, BGS clientinfo/name/team stores, and vehicle unlink gate checked. */
void ClientUserinfoChanged(int clientNum)
{
    gentity_t *ent = &g_entities[clientNum];
    gclient_t *client = ent->client;
    clientInfo_t *clientInfo = &bgs.clientinfo[clientNum];
    char userinfo[CLIENT_USERINFO_BUFFER_SIZE];
    char oldName[CLIENT_USERINFO_BUFFER_SIZE];

    trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));
    if (Info_Validate(userinfo) == 0) {
        strcpy(userinfo, CLIENT_BADINFO);
    }

    client->complaintDisabled = trap_IsLocalClient(clientNum);

    client->predictItems =
        atoi(Info_ValueForKey(userinfo, "cg_predictItems")) != 0;

    if (client->connectedState == CON_CONNECTED &&
        level.clientNameMode == SCRIPT_CLIENT_NAME_MODE_MANUAL) {
        ClientCleanName(Info_ValueForKey(userinfo, "name"),
                        client->cleanName,
                        CLIENT_NAME_SIZE);
    } else {
        Q_strncpyz(oldName, client->clientState.name, sizeof(oldName));
        ClientCleanName(Info_ValueForKey(userinfo, "name"),
                        client->clientState.name,
                        CLIENT_NAME_SIZE);
        Q_strncpyz(client->cleanName, client->clientState.name,
                   CLIENT_NAME_SIZE);
    }

    client->handicap = atoi(Info_ValueForKey(userinfo, "handicap"));
    if (client->handicap < 1 || client->handicap > 100) {
        client->handicap = 100;
    }

    clientInfo->clientNum = clientNum;
    Q_strncpyz(clientInfo->name, client->clientState.name,
               CLIENT_NAME_SIZE);

    if (clientInfo->team != client->clientState.team &&
        (client->ps.entityStateFlags & EF_IN_VEHICLE) != 0) {
        VEH_UnlinkPlayer(ent, 0);
    }

    clientInfo->team = client->clientState.team;
}
