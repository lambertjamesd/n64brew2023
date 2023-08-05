
local sk_definition_writer = require('sk_definition_writer')
local megatexture = require('tools.export_level.megatexture')
local collision = require('tools.export_level.collision')

sk_definition_writer.add_header('"levels/level_definition.h"')

sk_definition_writer.add_definition("world", "struct LevelDefinition", "_geo", {
    megatextureIndexes = sk_definition_writer.reference_to(megatexture.megatexture_indexes, 1),
    megatextureIndexCount = #megatexture.megatexture_indexes,

    collisionQuads = sk_definition_writer.reference_to(collision.colliders, 1),
    collisionQuadCount = #collision.colliders,
})