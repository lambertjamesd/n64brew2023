
local sk_definition_writer = require('sk_definition_writer')

local megatexture = require('tools.export_level.megatexture')

sk_definition_writer.add_definition("world", "struct LevelDefinition", "_geo", {
    megatextures = sk_definition_writer.reference_to(megatexture.megatexture_models, 1),
    megatextureCount = #megatexture.megatexture_models,
})