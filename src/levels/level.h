#ifndef __LEVELS_LEVEL_H__
#define __LEVELS_LEVEL_H__

#include "level_metadata.h"

extern struct LevelDefinition* gLoadedLevel;

void levelLoadDefinition(struct LevelMetadata* metadata);

#endif