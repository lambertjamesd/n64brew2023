#include "level.h"

#include "../util/rom.h"
#include "../util/memory.h"

struct LevelDefinition* gLoadedLevel;

void levelLoadDefinition(struct LevelMetadata* metadata) {
    u32 segmentSize = metadata->segmentRomEnd - metadata->segmentRomStart;
    void* levelSegment = malloc(segmentSize);
    romCopy(metadata->segmentRomStart, levelSegment, segmentSize);

    gLoadedLevel = levelDefinitionFixPointers(metadata->levelDefinition, (u32)levelSegment - (u32)metadata->segmentStart);
}