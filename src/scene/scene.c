#include "scene.h"

#include "../build/assets/models/chapel.h"
#include "../build/assets/materials/static.h"
#include "../levels/level.h"
#include "../megatextures/megatexture_renderer.h"

#include "../controls/controller.h"

#include "../util/time.h"

#define TILE_CACHE_ENTRY_COUNT  256

void sceneInit(struct Scene* scene) {
    cameraInit(&scene->camera, 70.0f, 0.125f * SCENE_SCALE, 20.0f * SCENE_SCALE);

    scene->camera.transform.position.x = 0.0f;
    scene->camera.transform.position.y = 1.0f;
    scene->camera.transform.position.z = 6.0f;

    // quatAxisAngle(&gUp, -M_PI * 0.5f, &scene->camera.transform.rotation);

    mtTileCacheInit(&scene->tileCache, TILE_CACHE_ENTRY_COUNT);

    for (int i = 0; i < gLoadedLevel->megatextureIndexcount; ++i) {
        megatexturePreload(&scene->tileCache, &gLoadedLevel->megatextureIndexes[i]);
    }
}

extern Vp fullscreenViewport;

void sceneRender(struct Scene* scene, struct RenderState* renderState, struct GraphicsTask* task) {
    megatextureRenderStart(&scene->tileCache);

    struct CameraMatrixInfo cameraInfo;
    cameraSetupMatrices(&scene->camera, renderState, (float)SCREEN_WD / SCREEN_HT, 1, &cameraInfo);
    cameraApplyMatrices(renderState, &cameraInfo);

    gSPDisplayList(renderState->dl++, static_vertex_color);
    gSPDisplayList(renderState->dl++, chapel_model_gfx);

    gSPDisplayList(renderState->dl++, static_tile_image);
    gDPSetTile(
        renderState->dl++, 
        G_IM_FMT_RGBA, 
        G_IM_SIZ_16b, 8, 0, 0, 0, 
        G_TX_CLAMP | G_TX_MIRROR, 5, 1, 
        G_TX_CLAMP | G_TX_MIRROR, 5, 1
    );
    gDPSetTileSize(renderState->dl++, 0, 32 << 2, 32 << 2, (32 + 31) << 2, (32 + 31) << 2);

    for (int i = 0; i < gLoadedLevel->megatextureIndexcount; ++i) {
        megatextureRender(&scene->tileCache, &gLoadedLevel->megatextureIndexes[i], &cameraInfo, renderState);
    }

    megatextureRenderEnd(&scene->tileCache);
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

void sceneUpdate(struct Scene* scene) {
    float frontToBack = 0.0f;
    float sideToSide = 0.0f;

    if (controllerGetButton(0, U_CBUTTONS)) {
        frontToBack -= 1.0f;
    }

    if (controllerGetButton(0, D_CBUTTONS)) {
        frontToBack += 1.0f;
    }

    if (controllerGetButton(0, R_CBUTTONS)) {
        sideToSide += 1.0f;
    }

    if (controllerGetButton(0, L_CBUTTONS)) {
        sideToSide -= 1.0f;
    }

    struct Vector3 forward;
    struct Vector3 right;
    playerGetMoveBasis(&scene->camera.transform, &forward, &right);

    vector3AddScaled(&scene->camera.transform.position, &forward, frontToBack * FIXED_DELTA_TIME, &scene->camera.transform.position);
    vector3AddScaled(&scene->camera.transform.position, &right, sideToSide * FIXED_DELTA_TIME, &scene->camera.transform.position);

    OSContPad* pad = controllersGetControllerData(0);

    // yaw
    struct Quaternion deltaRotate;
    quatAxisAngle(&gUp, -pad->stick_x * FIXED_DELTA_TIME * (1.0f / 80.0f), &deltaRotate);
    struct Quaternion tempRotation;
    quatMultiply(&deltaRotate, &scene->camera.transform.rotation, &tempRotation);

    // pitch
    quatAxisAngle(&gRight, pad->stick_y * FIXED_DELTA_TIME * (1.0f / 80.0f), &deltaRotate);
    quatMultiply(&tempRotation, &deltaRotate, &scene->camera.transform.rotation);
}