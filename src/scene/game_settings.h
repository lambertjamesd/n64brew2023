#ifndef __GAME_SETTINGS_H__
#define __GAME_SETTINGS_H__

struct GameSettings {
    int displayListLength;
    int tileCacheEntryCount;
    int highRes;
    float minLodBias;
};

extern struct GameSettings gUseSettings;

void gameSettingsConfigure(int hasExpansion);

#endif