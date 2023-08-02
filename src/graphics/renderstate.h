#ifndef __RENDER_STATE_H__
#define __RENDER_STATE_H__

#include <ultra64.h>

struct RenderState {
    Gfx* glist;
    Gfx* dl;
    u16* framebuffer;
    Gfx* currentMemoryChunk;
    int displayListLength;
};

void renderStateAlloc(struct RenderState* renderState, int displayListLength);
void renderStateInit(struct RenderState* renderState, u16* framebuffer);
Mtx* renderStateRequestMatrices(struct RenderState* renderState, unsigned count);
Light* renderStateRequestLights(struct RenderState* renderState, unsigned count);
Vp* renderStateRequestViewport(struct RenderState* renderState);
Vtx* renderStateRequestVertices(struct RenderState* renderState, unsigned count);
LookAt* renderStateRequestLookAt(struct RenderState* renderState);
void renderStateFlushCache(struct RenderState* renderState);
Gfx* renderStateAllocateDLChunk(struct RenderState* renderState, unsigned count);
Gfx* renderStateReplaceDL(struct RenderState* renderState, Gfx* nextDL);
Gfx* renderStateStartChunk(struct RenderState* renderState);
Gfx* renderStateEndChunk(struct RenderState* renderState, Gfx* chunkStart);

int renderStateMaxDLCount(struct RenderState* renderState);
int renderStateDidOverflow(struct RenderState* renderState);

void renderStateInlineBranch(struct RenderState* renderState, Gfx* dl);

#endif