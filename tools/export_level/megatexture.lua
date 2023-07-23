local sk_definition_writer = require('sk_definition_writer')
local sk_scene = require('sk_scene')
local sk_math = require('sk_math')
local sk_input = require('sk_input')

local function debug_print_recursive(any, line_prefix, already_visited)
    if type(any) == 'table' then
        local metatable = getmetatable(any)

        if metatable and metatable.__tostring then
            io.write(tostring(any))
            return
        end

        if already_visited[any] then
            io.write('<already printed>')
            return
        end

        already_visited[any] = true

        io.write('{\n')

        for k, v in pairs(any) do
            io.write('  ')
            io.write(line_prefix)
            debug_print_recursive(k, '  ' .. line_prefix, already_visited)
            io.write(' = ')
            debug_print_recursive(v, '  ' .. line_prefix, already_visited)
            io.write(',\n')
        end

        io.write(line_prefix)
        io.write('}')

        return
    end

    if type(any) == 'string' then
        io.write("'")
        io.write(any)
        io.write("'")
        return
    end

    io.write(tostring(any))
end

local function debug_print(...)
    for k, v in ipairs{...} do
        debug_print_recursive(v, '', {})
        io.write('\t')
    end
    io.write('\n')
end

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

local function determine_uv_basis(model)
    if not model.uv then
        error('Model does not have texture cooridnates')
    end

    -- define the origin relative to the image origin (top left)
    local origin = determine_uv_pos(model, sk_math.vector3(0, 1, 0))
    local right = determine_uv_pos(model, sk_math.vector3(1, 1, 0))
    local up = determine_uv_pos(model, sk_math.vector3(0, 0, 0))

    return {origin = origin, right = right - origin, up = up - origin}
end

local function build_tiles_at_lod(uv_basis, normal, edge_loops, texture, lod)
    local rows = {}
    local current_loops = edge_loops

    local up_normal = uv_basis.up:normalized()

    for y = 32, texture.height - 32, 32 do
        local clipping_plane = sk_math.plane3_with_point(up_normal, uv_basis.origin + uv_basis.up * (y / texture.height))
        local before
        
        before, current_loops = split_mesh_outline(current_loops, normal, clipping_plane)

        table.insert(rows, before)
    end

    table.insert(rows, current_loops)

    local mesh_tiles = {}
    local current_row = {}

    for _, row in pairs(rows) do
        table.insert(mesh_tiles, {})
        table.insert(current_row, row)
    end

    local right_normal = uv_basis.right:normalized()

    for x = 32, texture.width - 32, 32 do
        local clipping_plane = sk_math.plane3_with_point(right_normal, uv_basis.origin + uv_basis.right * (x / texture.width))

        for row_index = 1, #current_row do 
            local before
            before, current_row[row_index] = split_mesh_outline(current_row[row_index], normal, clipping_plane)
            table.insert(mesh_tiles[row_index], before)
        end
    end

    local texture_tiles = {}

    for y = 0, texture.height - 32, 32 do
        local row = {}

        for x = 0, texture.width - 32, 32 do
            table.insert(row, texture:crop(x, y, 32, 32))
        end

        table.insert(texture_tiles, row)
    end

    for row_index = 1, #current_row do 
        table.insert(mesh_tiles[row_index], current_row[row_index])
    end

    return {
        mesh_tiles = mesh_tiles,
        texture = texture,
        tile_count_x = #texture_tiles[1],
        tile_count_y = #texture_tiles,
        texture_tiles = texture_tiles,
        lod = lod,
    }
end

local function is_power_of_2(value) 
    if value <= 0 or math.floor(value) ~= value then
        return false
    end

    while value ~= 1 do
        if (value & 1) == 1 then
            return false
        end

        value = value >> 1
    end

    return true
end

local function build_megatexture_model(world_mesh)
    local texture = world_mesh.material.tiles[1].texture

    if not texture then
        error('texture not set')
    end 

    if not is_power_of_2(texture.width) or not is_power_of_2(texture.height) then
        error('texture size ' .. texture.width .. 'x' .. texture.height .. ' is not a power of 2')
    end

    local current_texture = texture
    local result = {}

    local uv_basis = determine_uv_basis(world_mesh)
    local edge_loops = build_mesh_outline(world_mesh)

    local lod = 1

    while current_texture.width >= 32 or current_texture.height >= 32 do
        table.insert(result, build_tiles_at_lod(uv_basis, world_mesh.normals[1], edge_loops, current_texture, lod))
        current_texture = current_texture:resize(current_texture.width >> 1, current_texture.height >> 1)
        lod = lod + 1
    end

    return {
        name = world_mesh.name,
        layers = result,
        uv_basis = uv_basis,
        normal = world_mesh.normals[1],
        texture = texture,
    }
end

local image_index = {}

local function get_tiles_reference(tile_layer)
    local key = tile_layer.texture.name .. '_' .. tile_layer.texture.width .. 'x' .. tile_layer.texture.height

    if image_index[key] then
        return image_index[key]
    end

    local image_data = {}

    for _, row in pairs(tile_layer.texture_tiles) do
        for _, tile in pairs(row) do
            local tile_data = tile:get_data()

            for _, element in pairs(tile_data) do
                table.insert(image_data, element)
            end
        end
    end

    sk_definition_writer.add_definition(key, 'u64[]', '_img', image_data)

    image_index[key] = sk_definition_writer.reference_to(image_data, 1)

    return image_index[key]
end

local function determine_vertex_mapping(previous_loop, loop, next_loop)
    local beginning_indices = {}
    local middle_indices = {}
    local end_indices = {}

    for index, vertex in ipairs(loop) do
        local priority = 0

        if previous_loop then
            for _, other in ipairs(previous_loop) do
                if other == vertex then
                    priority = -1
                end
            end
        end

        if next_loop then
            for _, other in ipairs(next_loop) do
                if other == vertex then
                    priority = 1
                end
            end
        end

        if priority == -1 then
            table.insert(beginning_indices, index)
        elseif priority == 0 then
            table.insert(middle_indices, index)
        else
            table.insert(end_indices, index)
        end
    end

    local new_to_old_index = {}

    for _, idx in ipairs(beginning_indices) do
        table.insert(new_to_old_index, idx)
    end

    for _, idx in ipairs(middle_indices) do
        table.insert(new_to_old_index, idx)
    end

    for _, idx in ipairs(end_indices) do
        table.insert(new_to_old_index, idx)
    end

    local old_to_new_index = {}

    for new, old in ipairs(new_to_old_index) do
        old_to_new_index[old] = new
    end 

    return {
        new_to_old_index = new_to_old_index,
        old_to_new_index = old_to_new_index,
        beginning_overlap = #beginning_indices,
        ending_overlap = #end_indices,
    }
end

local function convert_vertex(vertex, megatexture_model)
    local scaled = vertex * sk_input.settings.fixed_point_scale
    local scaled_normal = megatexture_model.normal * 127

    local right = megatexture_model.uv_basis.right
    local up = megatexture_model.uv_basis.up

    local uv_relative = vertex - megatexture_model.uv_basis.origin

    local u = uv_relative:dot(right) / right:magnitudeSqrd()
    local v = uv_relative:dot(up) / up:magnitudeSqrd()

    u = math.floor(u * megatexture_model.texture.width * (1 << 5) + 0.5)
    v = math.floor(v * megatexture_model.texture.height * (1 << 5) + 0.5)

    if u >= 0x8000 then
        u = 0x7FFF
    end

    if v >= 0x8000 then
        v = 0x7FFF
    end

    return {{
        {math.floor(scaled.x + 0.5), math.floor(scaled.y + 0.5), math.floor(scaled.z + 0.5)},
        0,
        {u, v},
        {math.floor(scaled_normal.x + 0.5), math.floor(scaled_normal.y + 0.5), math.floor(scaled_normal.z + 0.5), 255},
    }}
end

local function calc_bits_needed(width)
    local bits = 0

    width = width - 1

    while (width > 0) do 
        width = width >> 1
        bits = bits + 1
    end

    return bits
end

local function write_mesh_tiles(megatexture_model, layer)
    local vertices = {}
    local indices = {}
    local tiles = {}

    min_tile_x = layer.tile_count_x
    min_tile_y = layer.tile_count_y

    max_tile_x = 0
    max_tile_y = 0

    for y, row in ipairs(layer.mesh_tiles) do
        local previous_loop = nil

        for x, cell in ipairs(row) do
            -- todo combine holes
            local current_loop = cell[1]
            local next_loop = row[x + 1] and row[x + 1][1]

            if #current_loop > 0 then
                min_tile_x = math.min(min_tile_x, x)
                min_tile_y = math.min(min_tile_y, y)

                max_tile_x = math.max(max_tile_x, x)
                max_tile_y = math.max(max_tile_y, y)
            end

            local vertex_mapping = determine_vertex_mapping(previous_loop, current_loop, next_loop)

            previous_loop = current_loop

            local beginning_vertex = #vertices + 1 - vertex_mapping.beginning_overlap
            local beginning_index_length = #indices

            for new, old in ipairs(vertex_mapping.new_to_old_index) do
                if new > vertex_mapping.beginning_overlap then
                    table.insert(vertices, convert_vertex(current_loop[old], megatexture_model))
                end
            end

            -- TODO proper polygon fill instead of triangle fan
            for i = 3, #current_loop do
                table.insert(indices, vertex_mapping.old_to_new_index[1] - 1)
                table.insert(indices, vertex_mapping.old_to_new_index[i - 1] - 1)
                table.insert(indices, vertex_mapping.old_to_new_index[i] - 1)
            end

            table.insert(tiles, {
                startVertex = beginning_vertex - 1,
                startIndex = beginning_index_length,
                indexCount = #indices - beginning_index_length,
                vertexCount = #current_loop,
            })
        end
    end

    sk_definition_writer.add_definition(megatexture_model.name .. '_vertices_' .. layer.lod, 'Vtx[]', '_geo', vertices)
    sk_definition_writer.add_definition(megatexture_model.name .. '_indices_' .. layer.lod, 'u8[]', '_geo', indices)
    sk_definition_writer.add_definition(megatexture_model.name .. '_tiles_' .. layer.lod, 'struct MTMeshTile[]', '_geo', tiles)

    local tileXBits = calc_bits_needed(max_tile_x + 1 - min_tile_x)

    return {
        vertices = sk_definition_writer.reference_to(vertices, 1),
        indices = sk_definition_writer.reference_to(indices, 1),
        tiles = sk_definition_writer.reference_to(tiles, 1),

        minTileX = min_tile_x - 1,
        minTileY = min_tile_y - 1,
        maxTileX = max_tile_x,
        maxTileY = max_tile_y,
        tileXBits = tileXBits,
    }
end 

local function write_tile_index(mesh_name, megatexture_model)
    local layers = {}

    for _, layer in pairs(megatexture_model.layers) do
        table.insert(layers, {
            tileSource = get_tiles_reference(layer),
            xTiles = layer.tile_count_x,
            yTiles = layer.tile_count_y,
            mesh = write_mesh_tiles(megatexture_model, layer),
        })
    end

    sk_definition_writer.add_definition(mesh_name .. '_layers', 'struct MTTileLayer[]', '_geo', layers)

    return {
        layers = sk_definition_writer.reference_to(layers, 1),
        layerCount = #layers,
    }
end

local megatexture_indexes = {}

for _, node in pairs(sk_scene.nodes_for_type('@megatexture')) do
    if #node.node.meshes > 0 then
        local world_mesh = node.node.meshes[1]:transform(node.node.full_transformation)
        local megatexture_model = build_megatexture_model(world_mesh)
        local megatexture_index = write_tile_index(world_mesh.name, megatexture_model)
        table.insert(megatexture_indexes, megatexture_index) 
    end
end

sk_definition_writer.add_definition('indexes', 'struct MTTileIndex[]', '_geo', megatexture_indexes)

sk_definition_writer.add_header('<ultra64.h>')
sk_definition_writer.add_header('"megatextures/tile_index.h"')

return {
    megatexture_indexes = megatexture_indexes
}