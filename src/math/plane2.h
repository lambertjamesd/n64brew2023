#ifndef __MATH_PLANE2_H__
#define __MATH_PLANE2_H__

#include "vector2.h"

struct Plane2 {
    struct Vector2 normal;
    float d;
};

float plane2DistanceToPoint(struct Plane2* plane, struct Vector2* point);

#endif