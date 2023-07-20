#ifndef __SCENE_H__
#define __SCENE_H__

#include <ultra64.h>

#include "./camera.h"
#include "../graphics/renderstate.h"
#include "../graphics/graphics.h"

struct Scene {
    struct Camera camera;
};

void sceneInit(struct Scene* scene);
void sceneRender(struct Scene* scene, struct RenderState* renderState, struct GraphicsTask* task);
void sceneUpdate(struct Scene* scene);

#endif