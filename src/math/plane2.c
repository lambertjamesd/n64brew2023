#include "plane2.h"

float plane2DistanceToPoint(struct Plane2* plane, struct Vector2* point) {
    return vector2Dot(&plane->normal, point) + plane->d;
}