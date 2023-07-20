local sk_definition_writer = require('sk_definition_writer')
local sk_scene = require('sk_scene')
local sk_math = require('sk_math')

megatexture_models = {}

local function determine_uv_pos(model, uv_pos)

    local a = model.uv[2] - model.uv[1]
    local b = model.uv[3] - model.uv[1]

    -- | ox | = | ax bx |   | x |    | u |
    -- | oy |   | ay by | * | y | +  | v |

    --               | by -bx |   | ox - u |   | x |
    -- ax*by - ay*bx | -ay ax | * | oy - v | = | y |

    local determinant = a.x * b.y - a.y * b.x
    local uv_offset = uv_pos - model.uv[1]

    local x = determinant * (uv_offset.x * b.y - uv_offset.y * b.x)
    local y = determinant * (uv_offset.y * a.x - uv_offset.x * a.y)

    local edge_a = model.vertices[2] - model.vertices[1]
    local edge_b = model.vertices[3] - model.vertices[1]

    return model.vertices[1] + edge_a * x + edge_b * y
end

local function determine_model_basis(model)
    if not model.uv then
        error('Model does not have texture cooridnates')
    end

    local origin = determine_uv_pos(model, sk_math.vector3(0, 0, 0))
    local right = determine_uv_pos(model, sk_math.vector3(1, 0, 0))
    local up = determine_uv_pos(model, sk_math.vector3(0, 1, 0))

    return origin, right - origin, up - origin
end

for _, node in pairs(sk_scene.nodes_for_type('@megatexture')) do
    table.insert(megatexture_models, {1, 2, 3})

    if #node.node.meshes > 0 then
        print(determine_model_basis(node.node.meshes[1]:transform(node.node.full_transformation)))
    end
end


sk_definition_writer.add_definition("megatextures", "struct MegaTextureModel", "_geo", megatexture_models)

return {
    megatexture_models = megatexture_models
}