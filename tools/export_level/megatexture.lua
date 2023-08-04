local sk_definition_writer = require('sk_definition_writer')
local sk_scene = require('sk_scene')
local sk_math = require('sk_math')
local sk_input = require('sk_input')
local sk_transform = require('sk_transform')

local lod_reduction = 2

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

local POINT_ON_EDGE_THRESHOLD = 0.0001

local function distance_to_cutting_mesh(point, plane)
    local result = plane:distance_to_point(point)
    
    if math.abs(result) < POINT_ON_EDGE_THRESHOLD then
        return 0
    end

    return result
end

local function split_mesh_add_loop(behind_loops, front_loops, distance, result)
    result = result or {}

    if distance < 0 then
        table.insert(behind_loops, result)
    else
        table.insert(front_loops, result)
    end

    return result
end

local function split_mesh_loop(edge_loop, plane, edge_points)
    local current_loop = {}

    local first_loop = current_loop
    local behind_loops = {}
    local front_loops = {}

    local current_index = 1
    local prev_distance = 0
    local prev_point = nil
    local current_side = 0

    for point_index, point in ipairs(edge_loop) do
        local distance = distance_to_cutting_mesh(point, plane)

        if distance ~= 0 then
            current_index = point_index
            prev_distance = distance
            prev_point = point
            current_side = distance
            break
        end
    end

    if not prev_point then
        return {}, {}
    end

    current_index = loop_next_index(edge_loop, current_index)

    for i = 1, #edge_loop do
        local current_point = edge_loop[current_index]
        local current_distance = distance_to_cutting_mesh(current_point, plane)

        local skip_point = false

        if current_distance == 0 then
            local next_index = loop_next_index(edge_loop, current_index)
            local next_distance = distance_to_cutting_mesh(edge_loop[next_index], plane)

            if next_distance ~= 0 or prev_distance ~= 0 then
                local crossing_check = next_distance * prev_distance

                if crossing_check < 0 then
                    -- current point is on the plane with the prev and next points 
                    -- on either side of the plane
                    edge_points[current_point] = true
                    table.insert(current_loop, current_point)
                    current_loop = split_mesh_add_loop(behind_loops, front_loops, next_distance)
                    -- point is added to the new loop later on
                elseif crossing_check == 0 then
                    edge_points[current_point] = true

                    if next_distance * current_side < 0 then
                        current_loop = split_mesh_add_loop(behind_loops, front_loops, next_distance)
                        -- point is added to the new loop later on
                    end
                else
                    -- the loop just kissed the cutting plane, do nothing
                end
            else
                -- this skips any coplanar points along the cutting plane
                skip_point = true
            end
        elseif current_distance * prev_distance < 0 then
            -- the case where the line crosses the plane
            local total_distance = current_distance - prev_distance
            local lerp = current_distance / total_distance
            local new_point = current_point:lerp(prev_point, lerp)
            
            edge_points[new_point] = true
            table.insert(current_loop, new_point)
            current_loop = split_mesh_add_loop(behind_loops, front_loops, current_distance)
            table.insert(current_loop, new_point)
        end

        if not skip_point then
            table.insert(current_loop, current_point)
            prev_distance = current_distance
            prev_point = current_point
        end

        if current_distance ~= 0 then
            current_side = current_distance
        end

        current_index = loop_next_index(edge_loop, current_index)
    end

    if first_loop == current_loop then
        split_mesh_add_loop(behind_loops, front_loops, current_side, current_loop)
    else
        for _, point in ipairs(first_loop) do
            table.insert(current_loop, point)
        end
    end

    return behind_loops, front_loops
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
            current_vertex_index = loop_next_index(split_loops[current_loop_index], current_vertex_index)
        end

        current = split_loops[current_loop_index][current_vertex_index]
    end

    return result
end

local function combine_cut_loops(loops, edge_points, winding_direction)
    local edge_mapping = {}

    for loop_index, loop in ipairs(loops) do
        for point_index, point in ipairs(loop) do
            if edge_points[point] then
                table.insert(edge_mapping, { vertex = point, loop_index = loop_index, index = point_index, sort_key = winding_direction:dot(point)})
            end
        end
    end

    table.sort(edge_mapping, function(a, b) return a.sort_key < b.sort_key end)

    local next_edge_point = {}

    for index = 1,#edge_mapping,2 do
        local edge_point = edge_mapping[index]
        local next = edge_mapping[index + 1]

        if next then
            next_edge_point[edge_point.vertex] = {loop_index = next.loop_index, index = next.index}
        end
    end

    local result = {}
    local used_vertices = {}

    for loop_index = 1,#loops do
        for point_index = 1,#loops[loop_index] do
            local new_loop = build_loop_from_split(loop_index, point_index, loops, next_edge_point, used_vertices)

            if #new_loop > 0 then
                table.insert(result, new_loop)
            end
        end
    end

    return result
end

local function split_mesh_outline(edge_loops, normal, plane)
    local behind_loops = {}
    local infront_loops = {}
    local edge_points = {}

    for _, loop in ipairs(edge_loops) do
        local new_behind_loops, new_infront_loops = split_mesh_loop(loop, plane, edge_points)

        for _, loop in ipairs(new_behind_loops) do
            table.insert(behind_loops, loop)
        end

        for _, loop in ipairs(new_infront_loops) do
            table.insert(infront_loops, loop)
        end
    end

    winding_direction = normal:cross(plane.normal)

    return combine_cut_loops(behind_loops, edge_points, winding_direction), combine_cut_loops(infront_loops, edge_points, -winding_direction)
end

local function reduce(arr, reducer, initial)
    local result = initial

    for idx, value in pairs(arr) do
        result = reducer(result, value, idx)
    end

    return result
end

local function find_idx_in_dir(arr, dir)
    return reduce(arr, function (prev_idx, curr, curr_idx)
        if arr[prev_idx]:dot(dir) > curr:dot(dir) then
            return prev_idx
        else
            return curr_idx
        end
    end, 1)
end

local function determine_uv_basis(model)
    if not model.uv then
        error('Model does not have texture cooridnates')
    end

    local o_idx = find_idx_in_dir(model.uv, sk_math.vector3(-1, -1, 0))
    local r_idx = find_idx_in_dir(model.uv, sk_math.vector3(1, -1, 0))
    local u_idx = find_idx_in_dir(model.uv, sk_math.vector3(-1, 1, 0))

    -- define the origin relative to the image origin (top left)
    texture_cooridate_mtx = sk_transform.from_array({
        1,                      1,                     1,                     0,
        model.uv[o_idx].x,      model.uv[r_idx].x,     model.uv[u_idx].x,     0, 
        1 - model.uv[o_idx].y,  1 - model.uv[r_idx].y, 1 - model.uv[u_idx].y, 0, 
        0,                      0,                     0,                     1,
    })

    pos_matrix = sk_transform.from_array({
        model.vertices[o_idx].x,  model.vertices[r_idx].x, model.vertices[u_idx].x, 0, 
        model.vertices[o_idx].y,  model.vertices[r_idx].y, model.vertices[u_idx].y, 0, 
        model.vertices[o_idx].z,  model.vertices[r_idx].z, model.vertices[u_idx].z, 0, 
        0,                        0,                       0,                       1,
    })

    uv_basis_mtx = pos_matrix * texture_cooridate_mtx:inverse()

    local origin = sk_math.vector3(uv_basis_mtx[{1, 1}], uv_basis_mtx[{2, 1}], uv_basis_mtx[{3, 1}])
    local right = sk_math.vector3(uv_basis_mtx[{1, 2}], uv_basis_mtx[{2, 2}], uv_basis_mtx[{3, 2}])
    local up = sk_math.vector3(uv_basis_mtx[{1, 3}], uv_basis_mtx[{2, 3}], uv_basis_mtx[{3, 3}])

    return {origin = origin, right = right, up = up}
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
        error('texture not set for material ' .. world_mesh.material.name)
    end 

    if not is_power_of_2(texture.width) or not is_power_of_2(texture.height) then
        error('texture size ' .. texture.width .. 'x' .. texture.height .. ' is not a power of 2')
    end

    for i = 1, lod_reduction do
        texture = texture:resize(texture.width >> 1, texture.height >> 1)
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

    local data_index = 0

    for _, row in pairs(tile_layer.texture_tiles) do
        for _, tile in pairs(row) do
            local tile_data = tile:get_data()

            for _, element in pairs(tile_data) do
                table.insert(image_data, element)
                data_index = data_index + 1

                if data_index % 8 == 0 then
                    table.insert(image_data, sk_definition_writer.newline)
                end
            end

            table.insert(image_data, sk_definition_writer.newline)
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

    -- ensure the same order from previous loop 
    if previous_loop then
        for _, prev_point in ipairs(previous_loop) do
            for index, vertex in ipairs(loop) do
                if vertex == prev_point then
                    table.insert(beginning_indices, index)
                end
            end
        end
    end

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
            -- already added from previous loop
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
        local next_loop_point = index == #loop and loop[1] or loop[index + 1]

        local point_distance = plane_normal:distance_to_point(loop_point)
        local next_distance = plane_normal:distance_to_point(next_loop_point)

        if (point_distance >= 0 and next_distance < 0) or (point_distance <= 0 and next_distance > 0) then
            local total_distance = point_distance - next_distance

            local point_to_check = nil

            if point_distance == 0 then
                point_to_check = loop_point
            elseif total_distance ~= 0 then
                local lerp = point_distance / total_distance
                point_to_check = loop_point:lerp(next_loop_point, lerp)
            end

            if point_to_check and plane_tangent:distance_to_point(point_to_check) > 0 then
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

    for hole_loop_index, hole_loop in ipairs(holes) do
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
        table.insert(result, value)

        if idx_a == a_index then
            for idx_b_no_wrap = b_index,b_index+#b do
                if idx_b_no_wrap > #b then
                    table.insert(result, b[idx_b_no_wrap - #b])
                else
                    table.insert(result, b[idx_b_no_wrap])
                end
            end

            table.insert(result, value)
        end
    end

    return result
end

local function is_point_contained(triangle_vertices, point)
    return (triangle_vertices[1] - point):cross(triangle_vertices[2] - triangle_vertices[1]).z > 0 and
        (triangle_vertices[2] - point):cross(triangle_vertices[3] - triangle_vertices[2]).z > 0 and
        (triangle_vertices[3] - point):cross(triangle_vertices[1] - triangle_vertices[3]).z > 0
end

local function do_points_cross_plane(plane_tangent, plane_origin, a, b)
    local point_distance = (a - plane_origin):cross(plane_tangent).z
    local next_distance = (b - plane_origin):cross(plane_tangent).z

    return point_distance * next_distance <= 0
end

local function can_cut_vertex_at_index(vertices, loop, loop_index)
    local next_loop_index = loop_next_index(loop, loop_index)
    local prev_loop_index = loop_prev_index(loop, loop_index)

    local next_loop_point = vertices[loop[next_loop_index]]
    local loop_point = vertices[loop[loop_index]]
    local prev_loop_point = vertices[loop[prev_loop_index]]

    local triangle_vertices = {loop_point, next_loop_point, prev_loop_point}

    local edge = next_loop_point - prev_loop_point

    if (loop_point - prev_loop_point):cross(edge).z <= 0 then
        -- triangle winding is the wrong way
        return false
    end

    local current_index = loop_next_index(loop, next_loop_index)
    
    while current_index ~= prev_loop_index do
        if is_point_contained(triangle_vertices, vertices[loop[current_index]]) then
            return false
        end

        current_index = loop_next_index(loop, current_index)
    end 

    return true
end

local function fill_single_loop(vertices, normal, loop)
    local result = {}
    local next_index = 1
    local attempts = #loop

    local tangent = (vertices[2] - vertices[1]):normalized()
    local cotangent = normal:cross(tangent):normalized()

    local vertices_2d = {}

    for _, vertex in ipairs(vertices) do
        table.insert(vertices_2d, sk_math.vector3(
            tangent:dot(vertex),
            cotangent:dot(vertex),
            0
        ))
    end

    while #loop > 3 and attempts > 0 do
        if can_cut_vertex_at_index(vertices_2d, loop, next_index) then
            table.insert(result, {
                loop[loop_prev_index(loop, next_index)],
                loop[next_index],
                loop[loop_next_index(loop, next_index)],
            })

            table.remove(loop, next_index)

            if next_index > #loop then
                next_index = 1
            end

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
            hole_loops[next_join_spot.hole_loop_index].indices, 
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
            if does_loop_contain_point(megatexture_model, fill_loop.loop, loop.loop[1]) then
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
    local index_count = 0
    local previous_indices = {}
    local tiles = {}

    min_tile_x = layer.tile_count_x
    min_tile_y = layer.tile_count_y

    max_tile_x = 0
    max_tile_y = 0

    for y, row in ipairs(layer.mesh_tiles) do
        local current_mesh_data = nil
        local next_mesh_data = row[1] and fill_mesh(megatexture_model, row[1])
        local prev_overlap = nil

        for x, cell in ipairs(row) do
            current_mesh_data = next_mesh_data
            next_mesh_data = row[x + 1] and fill_mesh(megatexture_model, row[x + 1])

            local current_loop = current_mesh_data.vertices
            local current_loop_triangles = current_mesh_data.faces

            if #current_loop > 0 then
                min_tile_x = math.min(min_tile_x, x)
                min_tile_y = math.min(min_tile_y, y)

                max_tile_x = math.max(max_tile_x, x)
                max_tile_y = math.max(max_tile_y, y)
            end

            local next_loop = next_mesh_data and next_mesh_data.vertices
            local vertex_mapping = determine_vertex_mapping(prev_overlap, current_loop, next_loop)

            prev_overlap = {}

            for i = #current_loop - vertex_mapping.ending_overlap + 1, #current_loop do
                table.insert(prev_overlap, current_loop[vertex_mapping.new_to_old_index[i]])
            end

            local beginning_vertex = #vertices + 1 - vertex_mapping.beginning_overlap

            for new, old in ipairs(vertex_mapping.new_to_old_index) do
                if new > vertex_mapping.beginning_overlap then
                    table.insert(vertices, convert_vertex(current_loop[old], megatexture_model))
                end
            end

            local current_indices = {}

            for _, triangle in ipairs(current_loop_triangles) do
                table.insert(current_indices, vertex_mapping.old_to_new_index[triangle[1]] - 1)
                table.insert(current_indices, vertex_mapping.old_to_new_index[triangle[2]] - 1)
                table.insert(current_indices, vertex_mapping.old_to_new_index[triangle[3]] - 1)
            end

            local indices_as_string = table.concat(current_indices, ', ')

            local index_information = previous_indices[indices_as_string]

            if index_information then
                if #current_indices > 0 then
                    table.insert(indices, sk_definition_writer.comment(table.concat(current_indices, ', ') .. ' reused from previous entry'))
                    table.insert(indices, sk_definition_writer.newline)
                end
            else
                index_information = {
                    start_index = index_count,
                    index_count = #current_indices,
                }
                previous_indices[indices_as_string] = index_information

                for _, current in ipairs(current_indices) do
                    table.insert(indices, current)
                end

                index_count = index_count + #current_indices
                table.insert(indices, sk_definition_writer.newline)
            end

            table.insert(tiles, {
                startVertex = beginning_vertex - 1,
                startIndex = index_information.start_index,
                indexCount = index_information.index_count,
                vertexCount = #current_loop,
            })
        end

        table.insert(indices, sk_definition_writer.newline)
    end

    local tiles_x_bits = calc_bits_needed(max_tile_x + 1 - min_tile_x)

    local row_size = 1 << tiles_x_bits;

    local filtered_tiles = {}

    for index, tile in ipairs(tiles) do
        offset = index - 1
        x = (offset % layer.tile_count_x) + 1
        y = (offset // layer.tile_count_x) + 1

        if x >= min_tile_x and x <= max_tile_x and y >= min_tile_y and y <= max_tile_y then
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
        minUv = {x = layers[1].minTileX / imageLayers[1].xTiles, y = layers[1].minTileY / imageLayers[1].yTiles},
        maxUv = {x = layers[1].maxTileX / imageLayers[1].xTiles, y = layers[1].maxTileY / imageLayers[1].yTiles},
        worldPixelSize = math.sqrt(
            (megatexture_model.uv_basis.right:magnitude() / megatexture_model.texture.width) *
            (megatexture_model.uv_basis.up:magnitude() / megatexture_model.texture.height)
        ),
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