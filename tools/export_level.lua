
local sk_definition_writer = require('sk_definition_writer')
local megatexture = require('tools.export_level.megatexture')

sk_definition_writer.add_header('"levels/level_definition.h"')

sk_definition_writer.add_definition("world", "struct LevelDefinition", "_geo", {
    megatextureIndexes = sk_definition_writer.reference_to(megatexture.megatexture_indexes, 1),
    megatextureIndexcount = #megatexture.megatexture_indexes,
})