#ifndef __COLLISION_H__
#define __COLLISION_H__

#include "../levels/level_definition.h"

void collisionCollideSphere(struct CollisionQuad* quad, struct Vector3* origin, float radius);
int collisionCheckFloorHeight(struct CollisionQuad* quad, struct Vector3* origin, float* height);

#endif