#include "graphics.h"
#include "initgfx.h"
#include "util/memory.h"

int gScreenWidth = 320;
int gScreenHeight = 240;

struct GraphicsTask gGraphicsTasks[2];

extern OSMesgQueue  gfxFrameMsgQ;
extern OSMesgQueue	*schedulerCommandQueue;

void* gLevelSegment;

#if WITH_GFX_VALIDATOR
#include "../../gfxvalidator/validator.h"
#endif

#if WITH_DEBUGGER
#include "../../debugger/debugger.h"
#include "../../debugger/serial.h"

void graphicsOutputMessageToDebugger(char* message, unsigned len) {
    gdbSendMessage(GDBDataTypeText, message, len);
}

#endif

#define RDP_OUTPUT_SIZE 0x4000

u64* rdpOutput;
u64 __attribute__((aligned(16))) dram_stack[SP_DRAM_STACK_SIZE64 + 1];
u64 __attribute__((aligned(16))) gfxYieldBuf2[OS_YIELD_DATA_SIZE/sizeof(u64)];
u32 firsttime = 1;

u16* graphicsLayoutScreenBuffers(u16* memoryEnd) {
    gGraphicsTasks[0].framebuffer = memoryEnd - gScreenWidth * gScreenHeight;
    gGraphicsTasks[0].taskIndex = 0;
    gGraphicsTasks[0].msg.type = OS_SC_DONE_MSG;

    gGraphicsTasks[1].framebuffer = gGraphicsTasks[0].framebuffer - gScreenWidth * gScreenHeight;
    gGraphicsTasks[1].taskIndex = 1;
    gGraphicsTasks[1].msg.type = OS_SC_DONE_MSG;

    rdpOutput = (u64*)(gGraphicsTasks[1].framebuffer - RDP_OUTPUT_SIZE  / sizeof(u16));
    zeroMemory(rdpOutput, RDP_OUTPUT_SIZE);


    fullscreenViewport.vp.vscale[0] = gScreenWidth * 2;
    fullscreenViewport.vp.vscale[1] = gScreenHeight * 2;
    fullscreenViewport.vp.vscale[2] = G_MAXZ/4;
    fullscreenViewport.vp.vscale[3] = 0;

    fullscreenViewport.vp.vtrans[0] = gScreenWidth * 2;
    fullscreenViewport.vp.vtrans[1] = gScreenHeight * 2;
    fullscreenViewport.vp.vtrans[2] = G_MAXZ/4;
    fullscreenViewport.vp.vtrans[3] = 0;

    osWritebackDCache(&fullscreenViewport, sizeof(fullscreenViewport));

    return (u16*)rdpOutput;
}

void graphicsAlloc(int displayListLength) {
    renderStateAlloc(&gGraphicsTasks[0].renderState, displayListLength);
    renderStateAlloc(&gGraphicsTasks[1].renderState, displayListLength);
}

#define CLEAR_COLOR GPACK_RGBA5551(0x32, 0x5D, 0x79, 1)

int graphicsCreateTask(struct GraphicsTask* targetTask, GraphicsCallback callback, void* data) {
    struct RenderState *renderState = &targetTask->renderState;

    renderStateInit(renderState, targetTask->framebuffer);
    gSPSegment(renderState->dl++, 0, 0);
    gSPSegment(renderState->dl++, LEVEL_SEGMENT, gLevelSegment);

    gSPDisplayList(renderState->dl++, setup_rspstate);
    if (firsttime) {
        gSPDisplayList(renderState->dl++, rdpstateinit_dl);
	    firsttime = 0;
    }
    gSPDisplayList(renderState->dl++, setup_rdpstate);	
    gDPSetScissor(renderState->dl++, G_SC_NON_INTERLACE, 0, 0, gScreenWidth, gScreenHeight);
    
    gDPPipeSync(renderState->dl++);
    gDPSetColorImage(renderState->dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, gScreenWidth, osVirtualToPhysical(targetTask->framebuffer));

    gDPPipeSync(renderState->dl++);
    gDPSetCycleType(renderState->dl++, G_CYC_1CYCLE); 

    if (callback) {
        if (!callback(data, renderState, targetTask)) {
            return 0;
        }
    }

    gDPPipeSync(renderState->dl++);
    gDPFullSync(renderState->dl++);
    gSPEndDisplayList(renderState->dl++);

    if (renderStateDidOverflow(renderState)) {
        return 0;
    }

    renderStateFlushCache(renderState);

    OSScTask *scTask = &targetTask->task;

    OSTask_t *task = &scTask->list.t;

    task->data_ptr = (u64*)renderState->glist;
    task->data_size = (s32)renderState->dl - (s32)renderState->glist;
    task->type = M_GFXTASK;
    task->flags = OS_TASK_LOADABLE;
    task->ucode_boot = (u64*)rspbootTextStart;
    task->ucode_boot_size = (u32)rspbootTextEnd - (u32)rspbootTextStart;
    task->ucode = (u64*)gspF3DEX2_NoN_fifoTextStart;
    task->ucode_data = (u64*)gspF3DEX2_NoN_fifoDataStart;
    task->output_buff = (u64*)rdpOutput;
    task->output_buff_size = (u64*)rdpOutput + RDP_OUTPUT_SIZE/sizeof(u64);
    task->ucode_data_size = SP_UCODE_DATA_SIZE;
    task->dram_stack = (u64*)dram_stack;
    task->dram_stack_size = SP_DRAM_STACK_SIZE8;
    task->yield_data_ptr = (u64*)gfxYieldBuf2;
    task->yield_data_size = OS_YIELD_DATA_SIZE;

    scTask->flags = 
        OS_SC_NEEDS_RSP | 
        OS_SC_NEEDS_RDP | 
        OS_SC_LAST_TASK |
		OS_SC_SWAPBUFFER;
    scTask->framebuffer = targetTask->framebuffer;
    scTask->msg = &targetTask->msg;
    scTask->msgQ = &gfxFrameMsgQ;
    scTask->next = 0;
    scTask->state = 0;

#if WITH_GFX_VALIDATOR
#if WITH_DEBUGGER
    struct GFXValidationResult validationResult;
    zeroMemory(&validationResult, sizeof(struct GFXValidationResult));

    if (gfxValidate(&scTask->list, renderStateMaxDLCount(renderState), &validationResult) != GFXValidatorErrorNone) {
        gfxGenerateReadableMessage(&validationResult, graphicsOutputMessageToDebugger);
        gdbBreak();
    }

#endif // WITH_DEBUGGER
#endif // WITH_GFX_VALIDATOR

    osSendMesg(schedulerCommandQueue, (OSMesg)scTask, OS_MESG_BLOCK);
    return 1;
}
