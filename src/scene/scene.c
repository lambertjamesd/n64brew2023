#include "scene.h"

#include "../build/assets/models/chapel.h"
#include "../build/assets/materials/static.h"
#include "../levels/level.h"
#include "../megatextures/megatexture_renderer.h"

#include "../controls/controller.h"

#include "../util/time.h"

void sceneInit(struct Scene* scene) {
    cameraInit(&scene->camera, 70.0f, 0.5f * SCENE_SCALE, 30.0f * SCENE_SCALE);

    scene->camera.transform.position.x = 0.0f;
    scene->camera.transform.position.y = 1.0f;
    scene->camera.transform.position.z = 2.0f;
}

extern Vp fullscreenViewport;

int tmp = 0;

void sceneRender(struct Scene* scene, struct RenderState* renderState, struct GraphicsTask* task) {
    struct CameraMatrixInfo cameraInfo;
    cameraSetupMatrices(&scene->camera, renderState, (float)SCREEN_WD / SCREEN_HT, &fullscreenViewport, 0, &cameraInfo);
    cameraApplyMatrices(renderState, &cameraInfo);

    gSPDisplayList(renderState->dl++, static_vertex_color);
    gSPDisplayList(renderState->dl++, chapel_model_gfx);

    gSPDisplayList(renderState->dl++, static_tile_image);
    gDPSetTile(renderState->dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, 0, 0, G_TX_CLAMP | G_TX_MIRROR, 5, 0, G_TX_CLAMP | G_TX_MIRROR, 5, 0);
    megatextureRender(&gLoadedLevel->megatextureIndexes[0], renderState);

    ++tmp;
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
}