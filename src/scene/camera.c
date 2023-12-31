
#include "camera.h"
#include "math/transform.h"
#include "defs.h"
#include "../graphics/graphics.h"
#include "../math/mathf.h"

int isOutsideFrustrum(struct FrustrumCullingInformation* frustrum, struct Box3D* boundingBox) {
    for (int i = 0; i < frustrum->usedClippingPlaneCount; ++i) {
        struct Vector3 closestPoint;

        struct Vector3* normal = &frustrum->clippingPlanes[i].normal;

        closestPoint.x = normal->x < 0.0f ? boundingBox->min.x : boundingBox->max.x;
        closestPoint.y = normal->y < 0.0f ? boundingBox->min.y : boundingBox->max.y;
        closestPoint.z = normal->z < 0.0f ? boundingBox->min.z : boundingBox->max.z;

        if (planePointDistance(&frustrum->clippingPlanes[i], &closestPoint) < 0.00001f) {
            return 1;
        }
    }


    return 0;
}

int isSphereOutsideFrustrum(struct FrustrumCullingInformation* frustrum, struct Vector3* scaledCenter, float scaledRadius) {
    for (int i = 0; i < frustrum->usedClippingPlaneCount; ++i) {
        if (planePointDistance(&frustrum->clippingPlanes[i], scaledCenter) < -scaledRadius) {
            return 1;
        }
    }

    return 0;
}

void cameraInit(struct Camera* camera, float fov, float near, float far) {
    transformInitIdentity(&camera->transform);
    camera->fov = fov;
    camera->nearPlane = near;
    camera->farPlane = far;
}

void cameraBuildViewMatrix(struct Camera* camera, float matrix[4][4]) {
    struct Transform cameraTransCopy = camera->transform;
    vector3Scale(&cameraTransCopy.position, &cameraTransCopy.position, SCENE_SCALE);
    struct Transform inverse;
    transformInvert(&cameraTransCopy, &inverse);
    transformToMatrix(&inverse, matrix, 1.0f);
}

void cameraBuildProjectionMatrix(struct Camera* camera, float matrix[4][4], u16* perspectiveNormalize, float aspectRatio) {
    float planeScalar = 1.0f;

    if (camera->transform.position.y > camera->farPlane * 0.5f) {
        planeScalar = 2.0f * camera->transform.position.y / camera->farPlane;
    }

    guPerspectiveF(matrix, perspectiveNormalize, camera->fov, aspectRatio, camera->nearPlane * planeScalar, camera->farPlane * planeScalar, 1.0f);
}

void cameraExtractClippingPlane(float viewPersp[4][4], struct Plane* output, int axis, float direction) {
    output->normal.x = viewPersp[0][axis] * direction + viewPersp[0][3];
    output->normal.y = viewPersp[1][axis] * direction + viewPersp[1][3];
    output->normal.z = viewPersp[2][axis] * direction + viewPersp[2][3];
    output->d = viewPersp[3][axis] * direction + viewPersp[3][3];

    float mult = 1.0f / sqrtf(vector3MagSqrd(&output->normal));
    vector3Scale(&output->normal, &output->normal, mult);
    output->d *= mult * (1.0f / SCENE_SCALE);
}

int cameraIsValidMatrix(float matrix[4][4]) {
    return fabsf(matrix[3][0]) <= 0x7fff && fabsf(matrix[3][1]) <= 0x7fff && fabsf(matrix[3][2]) <= 0x7fff;
}

int cameraSetupMatrices(struct Camera* camera, struct RenderState* renderState, float aspectRatio, int extractClippingPlanes, struct CameraMatrixInfo* output) {
    float view[4][4];
    float combined[4][4];

	float fovy = camera->fov * 3.1415926 / 180.0;
    output->cotFov = cosf (fovy/2) / sinf (fovy/2);
    output->nearPlane = camera->nearPlane * (1.0f / SCENE_SCALE);
    output->farPlane = camera->farPlane * (1.0f / SCENE_SCALE);

    quatMultVector(&camera->transform.rotation, &gForward, &output->forwardVector);
    vector3Negate(&output->forwardVector, &output->forwardVector);
    output->cameraPosition = camera->transform.position;

    cameraBuildProjectionMatrix(camera, output->projectionMatrix, &output->perspectiveNormalize, aspectRatio);

    cameraBuildViewMatrix(camera, view);

    output->viewMtx = renderStateRequestMatrices(renderState, 1);

    if (!output->viewMtx) {
        return 0;
    }

    guMtxF2L(view, output->viewMtx);
    
    guMtxCatF(view, output->projectionMatrix, combined);

    if (!cameraIsValidMatrix(combined)) {
        goto error;
    }

    output->projectionView = renderStateRequestMatrices(renderState, 1);

    if (!output->projectionView) {
        return 0;
    }

    guMtxF2L(combined, output->projectionView);

    if (extractClippingPlanes) {
        cameraExtractClippingPlane(combined, &output->cullingInformation.clippingPlanes[0], 0, 1.0f);
        cameraExtractClippingPlane(combined, &output->cullingInformation.clippingPlanes[1], 0, -1.0f);
        cameraExtractClippingPlane(combined, &output->cullingInformation.clippingPlanes[2], 1, 1.0f);
        cameraExtractClippingPlane(combined, &output->cullingInformation.clippingPlanes[3], 1, -1.0f);
        output->cullingInformation.cameraPos = camera->transform.position;
        output->cullingInformation.usedClippingPlaneCount = 4;
    }

    return 1;
error:
    return 0;
}

int cameraApplyMatrices(struct RenderState* renderState, struct CameraMatrixInfo* matrixInfo) {
    Mtx* modelMatrix = renderStateRequestMatrices(renderState, 1);
    
    if (!modelMatrix) {
        return 0;
    }

    guMtxIdent(modelMatrix);
    gSPMatrix(renderState->dl++, osVirtualToPhysical(modelMatrix), G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);

    gSPMatrix(renderState->dl++, osVirtualToPhysical(matrixInfo->projectionView), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPPerspNormalize(renderState->dl++, matrixInfo->perspectiveNormalize);

    return 1;
}