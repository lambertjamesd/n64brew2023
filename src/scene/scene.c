#include "scene.h"

#include "../build/assets/models/chapel.h"
#include "../build/assets/materials/static.h"
#include "../levels/level.h"
#include "../megatextures/megatexture_renderer.h"

#include "../controls/controller.h"

#include "../util/time.h"
#include "game_settings.h"

void sceneInit(struct Scene* scene) {
    cameraInit(&scene->camera, 70.0f, 0.125f * SCENE_SCALE, 20.0f * SCENE_SCALE);

    scene->camera.transform.position.x = 0.0f;
    scene->camera.transform.position.y = 1.0f;
    scene->camera.transform.position.z = 6.0f;

    // quatAxisAngle(&gUp, -M_PI * 0.5f, &scene->camera.transform.rotation);

    mtTileCacheInit(&scene->tileCache, gUseSettings.tileCacheEntryCount);

    for (int i = 0; i < gLoadedLevel->megatextureIndexcount; ++i) {
        megatexturePreload(&scene->tileCache, &gLoadedLevel->megatextureIndexes[i]);
    }
}

extern Vp fullscreenViewport;

void sceneRenderDebug(struct Scene* scene, struct RenderState* renderState) {
    gSPDisplayList(renderState->dl++, static_solid_green);

    gDPFillRectangle(renderState->dl++, 32, 32, 32 + (int)(gMtLodBias * 32), 40);

    for (int i = 0; i < 6; ++i) {
        int y = 48 + i * 8;

        gDPFillRectangle(renderState->dl++, 32, y, 32 + scene->tileCache.tileRequests[i], y + 6);
    }

    gDPFillRectangle(renderState->dl++, 32, 120, 32 + scene->tileCache.failedRequestsThisFrame, 128);
}

int sceneRender(struct Scene* scene, struct RenderState* renderState, struct GraphicsTask* task) {

    struct CameraMatrixInfo cameraInfo;
    cameraSetupMatrices(&scene->camera, renderState, (float)gScreenWidth / gScreenHeight, 1, &cameraInfo);
    cameraApplyMatrices(renderState, &cameraInfo);

    gSPDisplayList(renderState->dl++, static_tile_image);

    return megatexturesRenderAll(&scene->tileCache, gLoadedLevel->megatextureIndexes, gLoadedLevel->megatextureIndexcount, &cameraInfo, renderState);
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

    struct Vector3 forward;
    struct Vector3 right;
    playerGetMoveBasis(&scene->camera.transform, &forward, &right);

    vector3AddScaled(&scene->camera.transform.position, &forward, frontToBack * FIXED_DELTA_TIME, &scene->camera.transform.position);
    vector3AddScaled(&scene->camera.transform.position, &right, sideToSide * FIXED_DELTA_TIME, &scene->camera.transform.position);

    OSContPad* pad = controllersGetControllerData(0);

    // yaw
    struct Quaternion deltaRotate;
    quatAxisAngle(&gUp, -pad->stick_x * ROTATE_SPEED * FIXED_DELTA_TIME * (1.0f / 80.0f), &deltaRotate);
    struct Quaternion tempRotation;
    quatMultiply(&deltaRotate, &scene->camera.transform.rotation, &tempRotation);

    // pitch
    quatAxisAngle(&gRight, pad->stick_y * ROTATE_SPEED * FIXED_DELTA_TIME * (1.0f / 80.0f), &deltaRotate);
    quatMultiply(&tempRotation, &deltaRotate, &scene->camera.transform.rotation);

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