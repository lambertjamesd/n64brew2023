#include "megatexture_culling_loop.h"

#include "../util/memory.h"
#include "../math/mathf.h"

struct Vector2 gStartingLoop[] = {
    {0.0f, 0.0f},
    {0.0f, 1.0f},
    {1.0f, 1.0f},
    {1.0f, 0.0f},
};

void mtCullingLoopInit(struct MTCullingLoop* loop, struct Vector2* min, struct Vector2* max) {
    memCopy(loop->loop, &gStartingLoop, sizeof(gStartingLoop));
    loop->loop[0].x = min->x;
    loop->loop[0].y = min->y;
    loop->loop[1].x = min->x;
    loop->loop[1].y = max->y;
    loop->loop[2].x = max->x;
    loop->loop[2].y = max->y;
    loop->loop[3].x = max->x;
    loop->loop[3].y = min->y;
    loop->loopSize = 4;
}

void mtCullingLoopClip(struct MTCullingLoop* loop, struct MTUVBasis* uvBasis, struct FrustrumCullingInformation* frustrum) {
    for (int i = 0; i < frustrum->usedClippingPlaneCount; ++i) {
        struct Plane2 clippingPlane;
        mtProjectClippingPlane(&frustrum->clippingPlanes[i], uvBasis, &clippingPlane);

        if (fabsf(clippingPlane.normal.x) < 0.000001f && fabsf(clippingPlane.normal.y) < 0.000001f) {
            if (clippingPlane.d < 0.0f) {
                // the plane is entirely outside the view
                return;
            }

            // the plane is entirely inside the view
            continue;
        }

        mtCullingLoopSplit(loop, &clippingPlane, NULL);

        if (loop->loopSize == 0) {
            // the plane is entirely outside the view
            return;
        }
    }
}

void mtCullingLoopAdd(struct MTCullingLoop* loop, struct Vector2* point) {
    if (!loop) {
        return;
    }

    loop->loop[loop->loopSize] = *point;
    ++loop->loopSize;
}

void mtCullingLoopSplit(struct MTCullingLoop* loop, struct Plane2* plane, struct MTCullingLoop* behindPlane) {
    struct MTCullingLoop frontPlane;

    if (behindPlane) {
        behindPlane->loopSize = 0;
    }
    frontPlane.loopSize = 0;

    for (int i = 0; i < loop->loopSize; ++i) {
        struct Vector2* currentPoint = &loop->loop[i];
        struct Vector2* nextPoint = &loop->loop[(i == loop->loopSize - 1) ? 0 : i + 1];

        float currentDistance = plane2DistanceToPoint(plane, currentPoint);

        if (fabsf(currentDistance) < 0.00001f) {
            mtCullingLoopAdd(behindPlane, currentPoint);
            mtCullingLoopAdd(&frontPlane, currentPoint);
            continue;
        }

        float nextDistance = plane2DistanceToPoint(plane, nextPoint);

        if (fabsf(currentDistance) < 0.00001f) {
            // point will be added to next iteration
            continue;
        }

        if (currentDistance < 0.0f) {
            mtCullingLoopAdd(behindPlane, currentPoint);
        } else {
            mtCullingLoopAdd(&frontPlane, currentPoint);
        }

        if (currentDistance * nextDistance < 0.0f) {
            float totalDistance = currentDistance - nextDistance;
            struct Vector2 clipPoint;
            vector2Lerp(currentPoint, nextPoint, currentDistance / totalDistance, &clipPoint);
            mtCullingLoopAdd(behindPlane, &clipPoint);
            mtCullingLoopAdd(&frontPlane, &clipPoint);
        }
    }

    // copy back into the loop
    for (int i = 0; i < frontPlane.loopSize; ++i) {
        loop->loop[i] = frontPlane.loop[i];
    }
    loop->loopSize = frontPlane.loopSize;
}


int mtCullingLoopTopIndex(struct MTCullingLoop* loop) {
    int result = 0;

    int nextPoint = 1;
    int prevPoint = loop->loopSize - 1;

    for (int i = 0; i < loop->loopSize; ++i) {
        if (loop->loop[nextPoint].y < loop->loop[result].y) {
            prevPoint = result;
            result = nextPoint;
            nextPoint = (nextPoint == loop->loopSize - 1) ? 0 : nextPoint + 1;
            continue;
        }

        if (loop->loop[prevPoint].y < loop->loop[result].y) {
            nextPoint = result;
            result = prevPoint;
            prevPoint = prevPoint == 0 ? loop->loopSize - 1 : prevPoint - 1;
            continue;
        }

        break;
    }

    return result;
}

float mtCullingLoopFindExtent(struct MTCullingLoop* loop, int* currentIndex, float* lastBoundaryExtent, float until, int direction) {
    float result = *lastBoundaryExtent;

    // infinite loop guard
    for (int i = 0; i < loop->loopSize; ++i) {
        int nextPointIndex = *currentIndex + direction;

        if (nextPointIndex == -1) {
            nextPointIndex = loop->loopSize - 1;
        } else if (nextPointIndex == loop->loopSize) {
            nextPointIndex = 0;
        }

        struct Vector2* currentPoint = &loop->loop[*currentIndex];

        if (currentPoint->y > until) {
            // if the boundary is before the current point
            // the exit early
            return result;
        }

        struct Vector2* nextPoint = &loop->loop[nextPointIndex];

        if (nextPoint->y < currentPoint->y) {
            return result;
        }

        if (nextPoint->y <= until || nextPoint->y == currentPoint->y) {
            if ((direction > 0) == (nextPoint->x < result)) {
                result = nextPoint->x;
            }

            *currentIndex = nextPointIndex;
            continue;
        }

        float lerpValue = mathfInvLerp(currentPoint->y, nextPoint->y, until);
        *lastBoundaryExtent = mathfLerp(currentPoint->x, nextPoint->x, lerpValue);

        if ((direction > 0) == (*lastBoundaryExtent < result)) {
            result = *lastBoundaryExtent;
        }

        break;
    }

    return result;
}

void mtCullingLoopFurthestPoint(struct MTCullingLoop* loop, struct Vector2* dir, struct Vector2* result) {
    float distance = vector2Dot(dir, &loop->loop[0]);
    *result = loop->loop[0];

    for (int i = 1; i < loop->loopSize; ++i) {
        float distanceCheck = vector2Dot(dir, &loop->loop[i]);

        if (distanceCheck > distance) {
            distance = distanceCheck;
            *result = loop->loop[i];
        }
    }
}

void mtProjectClippingPlane(struct Plane* plane, struct MTUVBasis* uvBasis, struct Plane2* result) {
    result->normal.x = vector3Dot(&plane->normal, &uvBasis->uvRight);
    result->normal.y = vector3Dot(&plane->normal, &uvBasis->uvUp);
    result->d = plane->d + vector3Dot(&plane->normal, &uvBasis->uvOrigin);
}