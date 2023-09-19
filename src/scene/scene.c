#include "scene.h"

#include "../build/assets/models/chapel.h"
#include "../build/assets/materials/static.h"
#include "../levels/level.h"
#include "../megatextures/megatexture_renderer.h"
#include "./collision.h"
#include "../math/mathf.h"

#include "../controls/controller.h"

#include "../build/src/audio/clips.h"

#include "../util/time.h"
#include "game_settings.h"

#define PLAYER_RADIUS   0.125f
#define PLAYER_HEAD_HEIGHT  3.0f
#define PLAYER_CROUCH_HEAD_HEIGHT  1.5f
#define PLAYER_HEAD_VELOCITY    3.0f
#define GROUND_HEIGHT           0.0f

#define GRAVITY     -9.8f

#define FADE_IN_DELAY   1.0f
#define FADE_IN_TIME    2.0f

void sceneInit(struct Scene* scene) {
    cameraInit(&scene->camera, 70.0f, 0.05f * SCENE_SCALE, 20.0f * SCENE_SCALE);

    scene->camera.transform.position.x = 0.0f;
    scene->camera.transform.position.y = PLAYER_HEAD_HEIGHT;
    scene->camera.transform.position.z = 14.0f;

    // quatAxisAngle(&gUp, -M_PI * 0.5f, &scene->camera.transform.rotation);

    mtTileCacheInit(&scene->tileCache, gUseSettings.tileCacheEntryCount);

    for (int i = 0; i < gLoadedLevel->megatextureIndexCount; ++i) {
        megatexturePreload(&scene->tileCache, &gLoadedLevel->megatextureIndexes[i], gUseSettings.minTileAxisTileCount);
    }

    scene->verticalVelocity = 0.0f;

    scene->fadeTimer = FADE_IN_DELAY + FADE_IN_TIME;
    
    scene->leftChannel = soundPlayerPlay(SOUNDS_MUSIC_L, 0.0f, 0.5f, 0);
    scene->rightChannel = soundPlayerPlay(SOUNDS_MUSIC_R, 0.0f, 0.5f, 127);
    // scene->rightChannel = -1;
}

extern Vp fullscreenViewport;

void sceneRenderDebug(struct Scene* scene, struct RenderState* renderState) {
    gSPDisplayList(renderState->dl++, static_solid_green);

    gDPFillRectangle(renderState->dl++, 64, 64, 64 + (int)(gMtLodBias * 32), 72);

    for (int i = 0; i < 6; ++i) {
        int y = 80 + i * 8;

        gDPFillRectangle(renderState->dl++, 64, y, 64 + scene->tileCache.tileRequests[i], y + 6);
    }

    gDPFillRectangle(renderState->dl++, 64, 152, 64 + scene->tileCache.totalTileRequests, 160);

    gDPSetPrimColor(renderState->dl++, 255, 255, 255, 0, 0, 255);
    gDPFillRectangle(renderState->dl++, 64, 178, 64 + scene->tileCache.overflowRequestCount, 186);
}

int sceneRender(struct Scene* scene, struct RenderState* renderState, struct GraphicsTask* task) {

    struct CameraMatrixInfo cameraInfo;
    cameraSetupMatrices(&scene->camera, renderState, (float)gScreenWidth / gScreenHeight, 1, &cameraInfo);
    cameraApplyMatrices(renderState, &cameraInfo);

    gSPDisplayList(renderState->dl++, static_tile_image);

    u8 color = 0;

    if (scene->fadeTimer < FADE_IN_TIME) {
        color = 255 - (u8)(255.0f * scene->fadeTimer / FADE_IN_TIME);
    }

    gDPSetPrimColor(renderState->dl++, 255, 255, color, color, color, 255);

    if (!megatexturesRenderAll(&scene->tileCache, gLoadedLevel->megatextureIndexes, gLoadedLevel->megatextureIndexCount, &cameraInfo, renderState)) {
        return 0;
    }

    // sceneRenderDebug(scene, renderState);

    return 1;
}

void playerGetMoveBasis(struct Transform* transform, struct Vector3* forward, struct Vector3* right) {
    quatMultVector(&transform->rotation, &gForward, forward);
    quatMultVector(&transform->rotation, &gRight, right);

    if (forward->y > 0.7f) {
        quatMultVector(&transform->rotation, &gUp, forward);
        vector3Negate(forward, forward);
    } else if (forward->y < -0.7f) {
        quatMultVector(&transform->rotation, &gUp, forward);
    }

    forward->y = 0.0f;
    right->y = 0.0f;

    vector3Normalize(forward, forward);
    vector3Normalize(right, right);
}

#define MOVE_SPEED      3.0f
#define ROTATE_SPEED    3.0f

void sceneUpdate(struct Scene* scene) {
    if (scene->fadeTimer > 0.0f) {
        scene->fadeTimer -= FIXED_DELTA_TIME;

        if (scene->fadeTimer < 0.0f) {
            scene->fadeTimer = 0.0f;
        }

        float soundVolume = 1.0f - (scene->fadeTimer / (FADE_IN_DELAY + FADE_IN_TIME));
        soundPlayerAdjustVolume(scene->rightChannel, soundVolume);
        soundPlayerAdjustVolume(scene->leftChannel, soundVolume);
    }

    if (!soundPlayerIsPlaying(scene->rightChannel) && !soundPlayerIsPlaying(scene->leftChannel)) {
        scene->leftChannel = soundPlayerPlay(SOUNDS_MUSIC_L, 1.0f, 0.5f, 0);
        scene->rightChannel = soundPlayerPlay(SOUNDS_MUSIC_R, 1.0f, 0.5f, 127);
    }

    float frontToBack = 0.0f;
    float sideToSide = 0.0f;

    if (controllerGetButton(0, U_CBUTTONS)) {
        frontToBack -= MOVE_SPEED;
    }

    if (controllerGetButton(0, D_CBUTTONS)) {
        frontToBack += MOVE_SPEED;
    }

    if (controllerGetButton(0, R_CBUTTONS)) {
        sideToSide += MOVE_SPEED;
    }

    if (controllerGetButton(0, L_CBUTTONS)) {
        sideToSide -= MOVE_SPEED;
    }

    OSContPad* pad2 = controllersGetControllerData(1);

    frontToBack -= pad2->stick_y * (MOVE_SPEED * (1.0f / 80.0f));
    sideToSide += pad2->stick_x * (MOVE_SPEED * (1.0f / 80.0f));

    struct Vector3 forward;
    struct Vector3 right;
    playerGetMoveBasis(&scene->camera.transform, &forward, &right);

    vector3AddScaled(&scene->camera.transform.position, &forward, frontToBack * FIXED_DELTA_TIME, &scene->camera.transform.position);
    vector3AddScaled(&scene->camera.transform.position, &right, sideToSide * FIXED_DELTA_TIME, &scene->camera.transform.position);

    OSContPad* pad = controllersGetControllerData(0);

    // Define a deadzone threshold (adjust this value as needed)
    float deadzoneThreshold = 10.0f;

    // Apply deadzone to stick input
    float stickX = pad->stick_x;
    float stickY = pad->stick_y;

    // Ignore values within the deadzone
    if (stickX >= -deadzoneThreshold && stickX <= deadzoneThreshold) {
        stickX = 0.0f; // Ignore small stick movements
    }

    if (stickY >= -deadzoneThreshold && stickY <= deadzoneThreshold) {
        stickY = 0.0f; // Ignore small stick movements
    }

    // yaw
    struct Quaternion deltaRotate;
    quatAxisAngle(&gUp, -stickX * ROTATE_SPEED * FIXED_DELTA_TIME * (1.0f / 80.0f), &deltaRotate);
    struct Quaternion tempRotation;
    quatMultiply(&deltaRotate, &scene->camera.transform.rotation, &tempRotation);

    // pitch
    quatAxisAngle(&gRight, stickY * ROTATE_SPEED * FIXED_DELTA_TIME * (1.0f / 80.0f), &deltaRotate);
    quatMultiply(&tempRotation, &deltaRotate, &scene->camera.transform.rotation);

    scene->verticalVelocity += GRAVITY * FIXED_DELTA_TIME;
    scene->camera.transform.position.y += scene->verticalVelocity * FIXED_DELTA_TIME;

    float headHeight = controllerGetButton(0, Z_TRIG) ? PLAYER_CROUCH_HEAD_HEIGHT : PLAYER_HEAD_HEIGHT;

    for (int i = 0; i < gLoadedLevel->collisionQuadCount; ++i) {
        collisionCollideSphere(&gLoadedLevel->collisionQuads[i], &scene->camera.transform.position, PLAYER_RADIUS);

        float contactPoint;
        if (collisionCheckFloorHeight(&gLoadedLevel->collisionQuads[i], &scene->camera.transform.position, &contactPoint)) {
            float height = scene->camera.transform.position.y - contactPoint;

            if (height < headHeight) {
                scene->verticalVelocity = 0.0f;
                scene->camera.transform.position.y = mathfMoveTowards(scene->camera.transform.position.y, contactPoint + headHeight, PLAYER_HEAD_VELOCITY * FIXED_DELTA_TIME);
            }
        }
    }

    if (scene->camera.transform.position.y - GROUND_HEIGHT < headHeight) {
        scene->verticalVelocity = 0.0f;
        scene->camera.transform.position.y = mathfMoveTowards(scene->camera.transform.position.y, headHeight + GROUND_HEIGHT, PLAYER_HEAD_VELOCITY * FIXED_DELTA_TIME);
    }

    if (controllerGetButtonDown(0, U_JPAD)) {
        gMtLodBias += 1.0f;
    }

    if (controllerGetButtonDown(0, D_JPAD)) {
        gMtLodBias -= 1.0f;
        if (gMtLodBias < 0.0f) {
            gMtLodBias = 0.0f;
        }
    }
}