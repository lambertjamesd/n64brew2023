#ifndef _CAMERA_H
#define _CAMERA_H

#include <ultra64.h>

#include "math/quaternion.h"
#include "math/vector3.h"
#include "math/transform.h"
#include "math/plane.h"
#include "graphics/renderstate.h"
#include "../math/boxs16.h"

#define MAX_CLIPPING_PLANE_COUNT    6

struct Camera {
    struct Transform transform;
    float nearPlane;
    float farPlane;
    float fov;
};

struct FrustrumCullingInformation {
    struct Plane clippingPlanes[MAX_CLIPPING_PLANE_COUNT];
    short usedClippingPlaneCount;

    struct Vector3 cameraPos;
};

struct CameraMatrixInfo {
    Mtx* projectionView;
    Mtx* viewMtx;
    float projectionMatrix[4][4];
    u16 perspectiveNormalize;
    struct FrustrumCullingInformation cullingInformation;

    struct Vector3 forwardVector;
    struct Vector3 cameraPosition;

    float cotFov;
    float nearPlane;
    float farPlane;
};

int isOutsideFrustrum(struct FrustrumCullingInformation* frustrum, struct BoundingBoxs16* boundingBox);
int isSphereOutsideFrustrum(struct FrustrumCullingInformation* frustrum, struct Vector3* scaledCenter, float scaledRadius);

void cameraInit(struct Camera* camera, float fov, float near, float far);
void cameraBuildViewMatrix(struct Camera* camera, float matrix[4][4]);
void cameraBuildProjectionMatrix(struct Camera* camera, float matrix[4][4], u16* perspectiveNorm, float aspectRatio);
int cameraSetupMatrices(struct Camera* camera, struct RenderState* renderState, float aspectRatio, int extractClippingPlanes, struct CameraMatrixInfo* output);

int cameraApplyMatrices(struct RenderState* renderState, struct CameraMatrixInfo* matrixInfo);

float cameraClipDistance(struct Camera* camera, float distance);

#endif