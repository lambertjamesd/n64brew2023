local sk_definition_writer = require('sk_definition_writer')
local sk_scene = require('sk_scene')
local sk_math = require('sk_math')
local sk_input = require('sk_input')

local function debug_print_recursive(any, line_prefix, already_visited)
    if type(any) == 'table' then
        local metatable = getmetatable(any)

        if metatable and metatable.__len then
            if already_visited[any] then
                io.write('<already printed>')
                return
            end
    
            already_visited[any] = true

            io.write('{\n')

            for i = 1, #any do
                io.write('  ')
                io.write(line_prefix)
                debug_print_recursive(i, '  ' .. line_prefix, already_visited)
                io.write(' = ')
                debug_print_recursive(any[i], '  ' .. line_prefix, already_visited)
                io.write(',\n')
            end

            io.write(line_prefix)
            io.write('}')

            return
        end

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

local POINT_ON_EDGE_THRESHOLD = 0.0000001

local function split_mesh_loop(edge_loop, plane)
    local loop_behind = {}
    local loop_ahead = {}
    local split_indices = {}

    for index, curr in pairs(edge_loop) do
        next = index < #edge_loop and edge_loop[index + 1] or edge_loop[1]

        local distance = plane:distance_to_point(curr)

        if math.abs(distance) < POINT_ON_EDGE_THRESHOLD then
            table.insert(loop_behind, curr)
            table.insert(loop_ahead, curr)

            distance = 0
        elseif distance < 0 then
            table.insert(loop_behind, curr)
        else
            table.insert(loop_ahead, curr)
        end

        local next_distance = plane:distance_to_point(next)
            
        if math.abs(next_distance) >= POINT_ON_EDGE_THRESHOLD and distance * next_distance < 0 then
            local total_distance = distance - next_distance
            local lerp = distance / total_distance
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

    a_dir = a:normalized()

    b = b - a_dir * a_dir:dot(b)
    local b_scale = a:magnitude() / b:magnitude()
    b = b * b_scale

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

    edge_a_dir = edge_a:normalized()

    edge_b = edge_b - edge_a_dir * edge_a_dir:dot(edge_b)
    edge_b = edge_b * b_scale

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

    for y = 0, texture.height - 1, 32 do
        local row = {}

        for x = 0, texture.width - 1, 32 do
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

local function is_fill_loop(megatexture_model, loop)
    local sum = sk_math.vector3(0, 0, 0)

    for i = 3, #loop do
        sum = sum + (loop[i - 1] - loop[1]):cross(loop[i] - loop[1])
    end

    return sum:dot(megatexture_model.normal) > 0
end

local function does_loop_contain_point(megatexture_model, loop, point)
    local normal_clip = loop[2] - loop[1]
    clip_plane_normal = normal_clip:normalized()
    clip_plane_tangent = clip_plane_normal:cross(megatexture_model.normal)

    local plane_normal = sk_math.plane3_with_point(clip_plane_normal, point)
    local plane_tangent = sk_math.plane3_with_point(clip_plane_tangent, point)
    local intersection_count = 0

    for index, loop_point in ipairs(loop) do
        local offset = loop_point - point
        local next_offset = (index == #loop and loop[1] or loop[index + 1]) - point

        local point_distance = plane_normal:distance_to_point(offset)
        local next_distance = plane_normal:distance_to_point(next_offset)

        if (point_distance >= 0 and next_distance < 0) or (point_distance <= 0 and next_distance > 0) then
            local total_distance = point_distance - next_distance

            local point_to_check = nil

            if point_distance == 0 then
                point_to_check = offset
            elseif total_distance ~= 0 then
                local lerp = point_distance / total_distance
                point_to_check = offset:lerp(next_offset)
            end

            if point_to_check and clip_plane_tangent:distance_to_point(point_to_check) > 0 then
                intersection_count = intersection_count + 1
            end
        end
    end

    return (intersection_count & 1) == 1
end

local function shallow_copy(t)
    local t2 = {}
    for k,v in pairs(t) do
      t2[k] = v
    end
    return t2
end

local function find_next_hole_join_spot(vertices, loop_indices, holes)
    local result = nil

    for hole_loop_index, hole_loop in holes do
        for loop_index_index, loop_index in ipairs(loop_indices) do
            for hole_index_index, hole_index in ipairs(hole_loop.indices) do
                local distance = (vertices[loop_index] - vertices[hole_index]):magnitudeSqrd()

                if result == nil or distance < result.distance then
                    result = {
                        distance = distance,
                        hole_loop_index = hole_loop_index,
                        loop_index_index = loop_index_index,
                        hole_index_index = hole_index_index,
                    }
                end
            end
        end
    end

    return result
end

local function join_loops(a, a_index, b, b_index)
    local result = {}

    for idx_a, value in ipairs(a) do
        table.insert(value)

        if idx_a == a_index then
            for idx_b_no_wrap = b_index,b_index+#b do
                if idx_b_no_wrap > #b then
                    table.insert(b[idx_b_no_wrap - #b])
                else
                    table.insert(b[idx_b_no_wrap])
                end
            end

            table.insert(b_index)
            table.insert(a_index)
        end
    end

    return result
end

local function loop_prev_index(loop, index)
    if index == 1 then
        return #loop
    else
        return index - 1
    end
end

local function loop_next_index(loop, index)
    if index == #loop then
        return 1
    else
        return index + 1
    end
end

local function can_cut_vertex_at_index(vertices, normal, loop, loop_index)
    local next_index = loop_next_index(loop, loop_index)
    local prev_index = loop_prev_index(loop, loop_index)

    local next_point = vertices[loop[next_index]]
    local curr_point = vertices[loop[loop_index]]
    local prev_point = vertices[loop[prev_index]]

    local edge = next_point - prev_point

    if (curr_point - prev_point):cross(edge):dot(normal) <= 0 then
        return false
    end

    return true
end

local function fill_single_loop(vertices, normal, loop)
    local result = {}
    local next_index = 1
    local attempts = #loop

    while #loop > 3 and attempts > 0 do
        if can_cut_vertex_at_index(vertices, normal, loop, next_index) then
            table.insert(result, {
                loop[loop_prev_index(loop, next_index)],
                loop[next_index],
                loop[loop_next_index(loop, next_index)],
            })

            table.remove(loop, next_index)

            attempts = #loop
        else
            next_index = loop_next_index(loop, next_index)
            attempts = attempts - 1
        end
    end

    table.insert(result, loop)

    return result
end

local function fill_single_loop_with_holes(vertices, normal, loop, hole_loops)
    local loop_indices = shallow_copy(loop.indices)
    hole_loops = shallow_copy(hole_loops)

    while #hole_loops > 0 do
        local next_join_spot = find_next_hole_join_spot(vertices, loop_indices, hole_loops)

        if not next_join_spot then
            hole_loops = {}
        end

        loop_indices = join_loops(
            loop_indices, 
            next_join_spot.loop_index_index, 
            hole_loops[next_join_spot.hole_loop_index], 
            next_join_spot.hole_index_index
        )

        table.remove(hole_loops, next_join_spot.hole_loop_index)
    end

    return fill_single_loop(vertices, normal, loop_indices)
end

local function build_indices(start_index, count)
    local result = {}
    for i = 0, count-1 do
        table.insert(result, i + start_index)
    end
    return result
end

local function fill_mesh(megatexture_model, loops)
    local vertices = {}
    local faces = {}

    local fill_loops = {}
    local all_hole_loops = {}

    for _, loop in ipairs(loops) do
        if is_fill_loop(megatexture_model, loop) then
            table.insert(fill_loops, { loop = loop, indices = build_indices(#vertices + 1, #loop)})
        else
            table.insert(all_hole_loops, { loop = loop, indices = build_indices(#vertices + 1, #loop)})
        end

        for _, vertex in ipairs(loop) do
            table.insert(vertices, vertex)
        end
    end

    for fill_loop_index, fill_loop in ipairs(fill_loops) do
        local hole_loops = {}

        for _, loop in ipairs(all_hole_loops) do
            if does_loop_contain_point(megatexture_model, fill_loop.loop, loop.loop) then
                table.insert(hole_loops, loop)
            end
        end

        local face_indices = fill_single_loop_with_holes(vertices, megatexture_model.normal, fill_loop, hole_loops)

        for _, value in ipairs(face_indices) do
            table.insert(faces, value)
        end
    end

    return { vertices = vertices, faces = faces }
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
        local current_mesh_data = nil
        local next_mesh_data = row[1] and fill_mesh(megatexture_model, row[1])

        for x, cell in ipairs(row) do
            local previous_loop = current_mesh_data and current_mesh_data.vertices
            current_mesh_data = next_mesh_data
            next_mesh_data = row[x + 1] and fill_mesh(megatexture_model, row[x + 1])

            local current_loop = current_mesh_data.vertices
            local current_loop_triangles = current_mesh_data.faces

            -- todo combine holes
            local current_loop = cell[1]
            local next_loop = row[x + 1] and row[x + 1][1]

            if not current_loop then
                current_loop = {}
            end

            if #current_loop > 0 then
                min_tile_x = math.min(min_tile_x, x)
                min_tile_y = math.min(min_tile_y, y)

                max_tile_x = math.max(max_tile_x, x)
                max_tile_y = math.max(max_tile_y, y)
            end

            local next_loop = next_mesh_data and next_mesh_data.vertices
            local vertex_mapping = determine_vertex_mapping(previous_loop, current_loop, next_loop)

            local beginning_vertex = #vertices + 1 - vertex_mapping.beginning_overlap
            local beginning_index_length = #indices

            for new, old in ipairs(vertex_mapping.new_to_old_index) do
                if new > vertex_mapping.beginning_overlap then
                    table.insert(vertices, convert_vertex(current_loop[old], megatexture_model))
                end
            end

            for _, triangle in ipairs(current_loop_triangles) do
                table.insert(indices, vertex_mapping.old_to_new_index[triangle[1]] - 1)
                table.insert(indices, vertex_mapping.old_to_new_index[triangle[2]] - 1)
                table.insert(indices, vertex_mapping.old_to_new_index[triangle[3]] - 1)
            end

            table.insert(tiles, {
                startVertex = beginning_vertex - 1,
                startIndex = beginning_index_length,
                indexCount = #indices - beginning_index_length,
                vertexCount = #current_loop,
            })
        end
    end

    local tiles_x_bits = calc_bits_needed(max_tile_x + 1 - min_tile_x)

    local tilex_x = 1 << tiles_x_bits;

    local filtered_tiles = {}

    for index, tile in ipairs(tiles) do
        offset = index - 1
        x = (offset % layer.tile_count_x) + 1
        y = (offset // layer.tile_count_x) + 1

        if x >= min_tile_x and x <= min_tile_x + tilex_x and y >= min_tile_y and y <= max_tile_y then
            table.insert(filtered_tiles, tile)
        end
    end

    sk_definition_writer.add_definition(megatexture_model.name .. '_vertices_' .. layer.lod, 'Vtx[]', '_geo', vertices)
    sk_definition_writer.add_definition(megatexture_model.name .. '_indices_' .. layer.lod, 'u8[]', '_geo', indices)
    sk_definition_writer.add_definition(megatexture_model.name .. '_tiles_' .. layer.lod, 'struct MTMeshTile[]', '_geo', filtered_tiles)

    return {
        vertices = sk_definition_writer.reference_to(vertices, 1),
        indices = sk_definition_writer.reference_to(indices, 1),
        tiles = sk_definition_writer.reference_to(filtered_tiles, 1),

        minTileX = min_tile_x - 1,
        minTileY = min_tile_y - 1,
        maxTileX = max_tile_x,
        maxTileY = max_tile_y,
        tileXBits = tiles_x_bits,
    }
end 

local function write_tile_index(world_mesh, megatexture_model)
    local layers = {}
    local imageLayers = {}

    for _, layer in pairs(megatexture_model.layers) do
        table.insert(layers, write_mesh_tiles(megatexture_model, layer))

        table.insert(imageLayers, {
            tileSource = get_tiles_reference(layer),
            xTiles = layer.tile_count_x,
            yTiles = layer.tile_count_y,
            isAlwaysLoaded = layer.tile_count_x <= 4 and layer.tile_count_y <= 4,
        })
    end

    sk_definition_writer.add_definition(world_mesh.name .. '__mesh_layers', 'struct MTMeshLayer[]', '_geo', layers)
    sk_definition_writer.add_definition(world_mesh.name .. '__image_layers', 'struct MTImageLayer[]', '_geo', imageLayers)

    return {
        meshLayers = sk_definition_writer.reference_to(layers, 1),
        imageLayers = sk_definition_writer.reference_to(imageLayers, 1),
        layerCount = #layers,
        boundingBox = world_mesh.bb,
        uvBasis = {
            uvOrigin = megatexture_model.uv_basis.origin,
            uvRight = megatexture_model.uv_basis.right,
            uvUp = megatexture_model.uv_basis.up,
            normal = world_mesh.normals[1],
        },
        worldPixelSize = megatexture_model.uv_basis.right:magnitude() / megatexture_model.texture.width,
    }
end

local megatexture_indexes = {}

for _, node in pairs(sk_scene.nodes_for_type('@megatexture')) do
    if #node.node.meshes > 0 then
        local world_mesh = node.node.meshes[1]:transform(node.node.full_transformation)
        print('processing ' .. world_mesh.name)
        local megatexture_model = build_megatexture_model(world_mesh)
        local megatexture_index = write_tile_index(world_mesh, megatexture_model)
        table.insert(megatexture_indexes, megatexture_index) 
    end
end

sk_definition_writer.add_definition('indexes', 'struct MTTileIndex[]', '_geo', megatexture_indexes)

sk_definition_writer.add_header('<ultra64.h>')
sk_definition_writer.add_header('"megatextures/tile_index.h"')

return {
    megatexture_indexes = megatexture_indexes
}