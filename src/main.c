
#include <ultra64.h>
#include <sched.h>

#include "defs.h"
#include "graphics/graphics.h"
#include "util/rom.h"
#include "scene/scene.h"
#include "util/time.h"
#include "util/memory.h"
#include "string.h"
#include "controls/controller.h"
#include "audio/soundplayer.h"
#include "audio/audio.h"
#include "sk64/skelatool_defs.h"
#include "sk64/skelatool_animator.h"
#include "levels/level.h"
#include "levels/level_list.h"
#include "scene/game_settings.h"

#ifdef WITH_DEBUGGER
#include "../debugger/debugger.h"
#endif

#define FORCE_4MB   0

static OSThread gameThread;
static OSThread initThread;

u64    mainStack[STACKSIZEBYTES/sizeof(u64)];
static u64 gameThreadStack[STACKSIZEBYTES/sizeof(u64)];
static u64 initThreadStack[STACKSIZEBYTES/sizeof(u64)];

static void gameProc(void *);
static void initProc(void *);

static OSMesg           PiMessages[DMA_QUEUE_SIZE];
static OSMesgQueue      PiMessageQ;

OSMesgQueue      gfxFrameMsgQ;
static OSMesg           gfxFrameMsgBuf[MAX_FRAME_BUFFER_MESGS];
static OSScClient       gfxClient;


OSSched scheduler;
u64            scheduleStack[OS_SC_STACKSIZE/8];
OSMesgQueue	*schedulerCommandQueue;

OSPiHandle	*gPiHandle;

void boot(void *arg) {
    osInitialize();

    gPiHandle = osCartRomInit();

    osCreateThread(
        &initThread, 
        1, 
        initProc, 
        NULL,
        (void *)(initThreadStack+(STACKSIZEBYTES/sizeof(u64))), 
		(OSPri)INIT_PRIORITY
    );

    osStartThread(&initThread);
}

static void initProc(void* arg) {
    osCreatePiManager(
        (OSPri) OS_PRIORITY_PIMGR, 
        &PiMessageQ,
        PiMessages,
        DMA_QUEUE_SIZE
    );

    osCreateThread(
        &gameThread, 
        6, 
        gameProc, 
        0, 
        gameThreadStack + (STACKSIZEBYTES/sizeof(u64)),
        (OSPri)GAME_PRIORITY
    );

    osStartThread(&gameThread);

    osSetThreadPri(NULL, 0);
    for(;;);
}

struct Scene gScene;

extern OSMesgQueue dmaMessageQ;

extern char _heapStart[];

extern char _animation_segmentSegmentRomStart[];

typedef void (*InitCallback)(void* data);
typedef void (*UpdateCallback)(void* data);

struct SceneCallbacks {
    void* data;
    InitCallback initCallback;
    GraphicsCallback graphicsCallback;
    UpdateCallback updateCallback;
};

struct SceneCallbacks gTestChamberCallbacks = {
    .data = &gScene,
    .initCallback = (InitCallback)&sceneInit,
    .graphicsCallback = (GraphicsCallback)&sceneRender,
    .updateCallback = (UpdateCallback)&sceneUpdate,
};

struct SceneCallbacks* gSceneCallbacks = &gTestChamberCallbacks;

static void gameProc(void* arg) {
    int memSize = FORCE_4MB ? (4 * 1024 * 1024) : osMemSize;

    gameSettingsConfigure(memSize >= 8 * 1024 * 1024);

    u8 schedulerMode = OS_VI_NTSC_LPF1;

    int fps = 60;

	switch (osTvType) {
		case 0: // PAL
			schedulerMode = gUseSettings.highRes ? OS_VI_FPAL_HPF1 : OS_VI_FPAL_LPF1;
            gScreenWidth = gUseSettings.highRes ? 640 : 320;
            gScreenHeight = gUseSettings.highRes ? 576 : 288;
            fps = 50;
			break;
		case 1: // NTSC
			schedulerMode = gUseSettings.highRes ? OS_VI_NTSC_HPF1 : OS_VI_NTSC_LPF1;
            gScreenWidth = gUseSettings.highRes ? 640 : 320;
            gScreenHeight = gUseSettings.highRes ? 480 : 240;
			break;
		case 2: // MPAL
            schedulerMode = gUseSettings.highRes ? OS_VI_MPAL_HPF1 : OS_VI_MPAL_LPF1;
            gScreenWidth = gUseSettings.highRes ? 640 : 320;
            gScreenHeight = gUseSettings.highRes ? 480 : 240;
			break;
	}

    osCreateScheduler(
        &scheduler,
        (void *)(scheduleStack + OS_SC_STACKSIZE/8),
        SCHEDULER_PRIORITY,
        schedulerMode,
        1
    );

    schedulerCommandQueue = osScGetCmdQ(&scheduler);

    osCreateMesgQueue(&gfxFrameMsgQ, gfxFrameMsgBuf, MAX_FRAME_BUFFER_MESGS);
    osScAddClient(&scheduler, &gfxClient, &gfxFrameMsgQ);

	osViSetSpecialFeatures(OS_VI_GAMMA_OFF |
			OS_VI_GAMMA_DITHER_OFF |
			OS_VI_DIVOT_OFF |
			OS_VI_DITHER_FILTER_OFF);

    osViBlack(1);

    u32 pendingGFX = 0;
    u32 drawBufferIndex = 0;
    u8 frameControl = 0;
    u8 inputIgnore = 5;
    u8 drawingEnabled = 0;

    u16* memoryEnd = graphicsLayoutScreenBuffers((u16*)PHYS_TO_K0(memSize));

    gAudioHeapBuffer = (u8*)memoryEnd - AUDIO_HEAP_SIZE;

    memoryEnd = (u16*)gAudioHeapBuffer;

    heapInit(_heapStart, memoryEnd);
    graphicsAlloc(gUseSettings.displayListLength);
    romInit();

#ifdef WITH_DEBUGGER
    OSThread* debugThreads[2];
    debugThreads[0] = &gameThread;
    gdbInitDebugger(gPiHandle, &dmaMessageQ, debugThreads, 1);
#endif

    controllersInit();
    initAudio(fps);
    soundPlayerInit();
    levelLoadDefinition(&gLevelList[0]);
    gSceneCallbacks->initCallback(gSceneCallbacks->data);

    calculateBytesFree();

    while (1) {
        OSScMsg *msg = NULL;
        osRecvMesg(&gfxFrameMsgQ, (OSMesg*)&msg, OS_MESG_BLOCK);
        
        switch (msg->type) {
            case (OS_SC_RETRACE_MSG):
                // control the framerate
                frameControl = (frameControl + 1) % (FRAME_SKIP + 1);
                if (frameControl != 0) {
                    break;
                }

                if (pendingGFX < 2 && drawingEnabled) {
                    if (graphicsCreateTask(&gGraphicsTasks[drawBufferIndex], gSceneCallbacks->graphicsCallback, gSceneCallbacks->data)) {
                        drawBufferIndex = drawBufferIndex ^ 1;
                        ++pendingGFX;
                    }
                }

                controllersTriggerRead();
                controllerHandlePlayback();
                
                if (inputIgnore) {
                    --inputIgnore;
                } else {
                    gSceneCallbacks->updateCallback(gSceneCallbacks->data);
                    drawingEnabled = 1;
                }
                timeUpdateDelta();
                soundPlayerUpdate();
                controllersSavePreviousState();

                break;

            case (OS_SC_DONE_MSG):
                --pendingGFX;
                break;
            case (OS_SC_PRE_NMI_MSG):
                pendingGFX += 2;
                break;
            case SIMPLE_CONTROLLER_MSG:
                controllersReadPendingData();
                break;
        }
    }
}