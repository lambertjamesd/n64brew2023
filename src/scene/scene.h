#ifndef __SCENE_H__
#define __SCENE_H__

#include <ultra64.h>

#include "./camera.h"
#include "../graphics/renderstate.h"
#include "../graphics/graphics.h"
#include "../megatextures/megatexture_tilecache.h"

struct Scene {
    struct Camera camera;
    struct MTTileCache tileCache;
};

void sceneInit(struct Scene* scene);
int sceneRender(struct Scene* scene, struct RenderState* renderState, struct GraphicsTask* task);
void sceneUpdate(struct Scene* scene);

#endif