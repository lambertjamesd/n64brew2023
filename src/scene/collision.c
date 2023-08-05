
#include "./collision.h"
#include "../math/mathf.h"

void collisionCollideSphere(struct CollisionQuad* quad, struct Vector3* origin, float radius) {
    float planeDistance = planePointDistance(&quad->plane, origin);

    if (fabsf(planeDistance) > radius) {
        return;
    }

    struct Vector3 relative;
    vector3Sub(origin, &quad->corner, &relative);

    float aDistance = vector3Dot(&relative, &quad->edgeA);

    if (aDistance < -radius || aDistance > quad->edgeALength + radius) {
        return;
    }

    float bDistnace = vector3Dot(&relative, &quad->edgeB);

    if (bDistnace < -radius || bDistnace > quad->edgeBLength + radius) {
        return;
    }

    struct Vector3 closestPoint;
    vector3Scale(&quad->edgeA, &closestPoint, aDistance);
    vector3AddScaled(&closestPoint, &quad->edgeB, bDistnace, &closestPoint);

    struct Vector3 pushDir;
    vector3Sub(&relative, &closestPoint, &pushDir);
    float distanceSqrd = vector3MagSqrd(&pushDir);

    if (distanceSqrd > radius * radius) {
        return;
    }

    float distance = sqrtf(distanceSqrd);

    if (distance < 0.00001f) {
        vector3AddScaled(origin, &quad->plane.normal, radius, origin);
        return;
    }

    vector3AddScaled(&closestPoint, &pushDir, radius / distance, origin);
    vector3Add(origin, &quad->corner, origin);
}

int collisionCheckFloorHeight(struct CollisionQuad* quad, struct Vector3* origin, float* height) {
    if (fabsf(quad->plane.normal.y) < 0.001f) {
        return 0;
    }

    struct Vector3 contactPoint = *origin;
    contactPoint.y = (quad->plane.normal.x * origin->x + quad->plane.normal.z * origin->z + quad->plane.d) / -quad->plane.normal.y;

    struct Vector3 relative;
    vector3Sub(&contactPoint, &quad->corner, &relative);

    float aDistance = vector3Dot(&relative, &quad->edgeA);

    if (aDistance < 0 || aDistance > quad->edgeALength) {
        return 0;
    }

    float bDistnace = vector3Dot(&relative, &quad->edgeB);

    if (bDistnace < 0 || bDistnace > quad->edgeBLength) {
        return 0;
    }

    *height = contactPoint.y;

    return 1;
}