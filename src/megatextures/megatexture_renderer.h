#ifndef __MEGATEXTURE_RENDERER_H__
#define __MEGATEXTURE_RENDERER_H__

#include "tile_index.h"
#include "../graphics/renderstate.h"
#include "./megatexture_tilecache.h"
#include "../scene/camera.h"

void megatextureRenderStart(struct MTTileCache* tileCache);
int megatextureRender(struct MTTileCache* tileCache, struct MTTileIndex* index, struct CameraMatrixInfo* cameraInfo, struct RenderState* renderState);
void megatexturePreload(struct MTTileCache* tileCache, struct MTTileIndex* index);
void megatextureRenderEnd(struct MTTileCache* tileCache, int success);

#endif