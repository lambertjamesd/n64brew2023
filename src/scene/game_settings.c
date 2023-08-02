#include "game_settings.h"

#include "../megatextures/megatexture_renderer.h"

struct GameSettings gUseSettings;

void gameSettingsConfigure(int hasExpansion) {
    gUseSettings.displayListLength = hasExpansion ? 3600 : 14400;
    gUseSettings.tileCacheEntryCount = hasExpansion ? 512 : 2048;
    gUseSettings.highRes = hasExpansion ? 1 : 0;
    gUseSettings.minLodBias = hasExpansion ? 1.0f : 0.0f;

    gMtMinLoadBias = gUseSettings.minLodBias;
}