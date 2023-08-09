#include "game_settings.h"

#include "../megatextures/megatexture_renderer.h"

struct GameSettings gUseSettings;

void gameSettingsConfigure(int hasExpansion) {
    gUseSettings.displayListLength = hasExpansion ? 14400 : 3600;
    gUseSettings.tileCacheEntryCount = hasExpansion ? 2048 : 1024;
    gUseSettings.highRes = hasExpansion ? 1 : 0;
    gUseSettings.minLodBias = hasExpansion ? 0.0f : 0.0f;

    gMtMinLoadBias = gUseSettings.minLodBias;
}