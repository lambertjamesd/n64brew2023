local sk_definition_writer = require('sk_definition_writer')
local sk_scene = require('sk_scene')
local sk_math = require('sk_math')

megatexture_models = {}

local function edge_key(a, b)
    if a > b then
        a, b = b, a
    end
    
    return a .. ',' .. b
end

local function increment_edge_use(edge_use_count, a, b)
    local key = edge_key(a, b)
    edge_use_count[key] = (edge_use_count[key] or 0) + 1
end

local function add_if_single_edge(edges, edge_use_count, a, b)
    if edge_use_count[edge_key(a, b)] ~= 1 then
        return
    end

    table.insert(edges, {a, b})
end

local function build_edge_loop(point_to_edge, starting_edge, vertices)
    local result = {}

    local current_edge = starting_edge

    while point_to_edge[current_edge[2]] do
        table.insert(result, vertices[current_edge[1]])
        local next_vertex = current_edge[2]
        current_edge = point_to_edge[next_vertex]
        point_to_edge[next_vertex] = nil
    end

    return result
end

local function build_mesh_outline(model)
    local edge_use_count = {}

    for _, triangle in pairs(model.faces) do
        increment_edge_use(edge_use_count, triangle[1], triangle[2])
        increment_edge_use(edge_use_count, triangle[2], triangle[3])
        increment_edge_use(edge_use_count, triangle[3], triangle[1])
    end

    local edges = {}

    for _, triangle in pairs(model.faces) do
        add_if_single_edge(edges, edge_use_count, triangle[1], triangle[2])
        add_if_single_edge(edges, edge_use_count, triangle[2], triangle[3])
        add_if_single_edge(edges, edge_use_count, triangle[3], triangle[1])
    end

    local point_to_edge = {}

    for _, edge in pairs(edges) do
        point_to_edge[edge[1]] = edge
    end

    local edge_loops = {}

    for _, edge in pairs(edges) do
        local edge_loop = build_edge_loop(point_to_edge, edge, model.vertices)

        if #edge_loop > 0 then
            table.insert(edge_loops, edge_loop)
        end
    end

    return edge_loops
end

local function split_mesh_loop(edge_loop, plane)
    local loop_behind = {}
    local loop_ahead = {}
    local split_indices = {}

    for index, curr in pairs(edge_loop) do
        next = index < #edge_loop and edge_loop[index + 1] or edge_loop[1]

        local distance = plane:distance_to_point(curr)

        if distance < 0 then
            table.insert(loop_behind, curr)
        else
            table.insert(loop_ahead, curr)
        end

        local next_distance = plane:distance_to_point(next)

        if distance * next_distance <= 0 then
            local lerp = 0
            local total_distance = distance - next_distance

            if math.abs(total_distance) < 0.0000001 then
                lerp = 0.5
            else
                lerp = distance / total_distance
            end

            local new_point = curr:lerp(next, lerp)

            table.insert(loop_behind, new_point)
            table.insert(loop_ahead, new_point)
            table.insert(split_indices, {#loop_behind, #loop_ahead})
        end
    end

    return loop_behind, loop_ahead, split_indices
end

local function build_loop_from_split(current_loop_index, current_vertex_index, split_loops, next_edge_point, used_vertices)
    local result = {}

    local current = split_loops[current_loop_index][current_vertex_index]

    while not used_vertices[current] do
        used_vertices[current] = true
        table.insert(result, current)

        local next_on_split = next_edge_point[current]

        if next_on_split then
            current_loop_index = next_on_split.loop_index
            current_vertex_index = next_on_split.index
        else
            if current_vertex_index == #split_loops[current_loop_index] then
                current_vertex_index = 1
            else
                current_vertex_index = current_vertex_index + 1
            end
        end

        current = split_loops[current_loop_index][current_vertex_index]
    end

    return result
end

local function split_mesh_outline(edge_loops, normal, plane)
    local behind_loops = {}
    local infront_loops = {}

    local edge_points = {}

    local sort_axis = normal:cross(plane.normal)

    for loop_index, loop in pairs(edge_loops) do
        local behind, infront, indices = split_mesh_loop(loop, plane)
        table.insert(behind_loops, behind)
        table.insert(infront_loops, infront)

        for _, index in pairs(indices) do
            local vertex = behind[index[1]]
            table.insert(edge_points, {vertex = vertex, sort_key = vertex:dot(sort_axis), loop_index = loop_index, indices = index})
        end
    end

    table.sort(edge_points, function(a, b) return a.sort_key < b.sort_key end)

    local next_edge_point = {}
    local prev_edge_point = {}

    for index, edge_point in pairs(edge_points) do
        local next = index < #edge_points and edge_points[index + 1] or nil

        if next then
            next_edge_point[edge_point.vertex] = {loop_index = next.loop_index, index = next.indices[1]}
            prev_edge_point[next.vertex] = {loop_index = edge_point.loop_index, index = edge_point.indices[2]}
        end
    end

    local used_vertices = {}

    local result_behind = {}
    local result_infront = {}

    for loop_index, loop in pairs(behind_loops) do
        for index, _ in pairs(loop) do
            local new_loop = build_loop_from_split(loop_index, index, behind_loops, next_edge_point, used_vertices)

            if #new_loop > 0 then
                table.insert(result_behind, new_loop)
            end
        end
    end

    used_vertices = {}

    for loop_index, loop in pairs(infront_loops) do
        for index, _ in pairs(loop) do
            local new_loop = build_loop_from_split(loop_index, index, infront_loops, prev_edge_point, used_vertices)

            if #new_loop > 0 then
                table.insert(result_infront, new_loop)
            end
        end
    end

    return result_behind, result_infront
end

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
        local world_mesh = node.node.meshes[1]:transform(node.node.full_transformation)
        local edge_loops = build_mesh_outline(world_mesh)
        local behind, infront = split_mesh_outline(edge_loops, world_mesh.normals[1], sk_math.plane3(sk_math.vector3(1, 0, 0), 0))
        print(world_mesh.material.tiles[1].texture.height)
        print(determine_model_basis(world_mesh))
    end
end


sk_definition_writer.add_definition("megatextures", "struct MegaTextureModel", "_geo", megatexture_models)

return {
    megatexture_models = megatexture_models
}