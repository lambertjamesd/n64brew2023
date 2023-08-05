
local sk_definition_writer = require('sk_definition_writer')
local sk_scene = require('sk_scene')
local sk_mesh = require('sk_mesh')
local sk_math = require('sk_math')

local SAME_TOLERANCE = 0.00001

local function bottom_right_compare(a, b)
    if (math.abs(a.x - b.x) > SAME_TOLERANCE) then
        return a.x < b.x
    end

    if (math.abs(a.y - b.y) > SAME_TOLERANCE) then
        return a.y < b.y
    end

    return a.z < b.z
end

local function find_min(array, predicate)
    local result = array[1]
    local result_index = 1

    for index, current in pairs(array) do
        if (predicate(current, result)) then
            result = current
            result_index = index
        end
    end

    return result, result_index
end

local function bottom_right_most_index(vertices)
    return find_min(vertices, bottom_right_compare)
end

local function find_most_opposite_edge(from_edge, edges)
    return find_min(edges, function(a, b)
        return a:dot(from_edge) < b:dot(from_edge)
    end)
end

local function find_adjacent_vertices(mesh, corner_index)
    local result = {}

    for _, face in pairs(mesh.faces) do
        for index_index, index in pairs(face) do
            if (index == corner_index) then
                local next_index = index_index + 1

                if (next_index > #face) then
                    next_index = 1
                end

                local prev_index = index_index - 1

                if (prev_index == 0) then
                    prev_index = #face
                end

                result[face[next_index]] = true
                result[face[prev_index]] = true
            end
        end
    end

    return result
end

local function create_collision_quad(mesh)
    local bottom_right_most = mesh.vertices[1]

    local corner_point, corner_index = bottom_right_most_index(mesh.vertices)

    local adjacent_indices = find_adjacent_vertices(mesh, corner_index)

    local edges_from_corner = {}

    for index, _ in pairs(adjacent_indices) do
        table.insert(edges_from_corner, mesh.vertices[index] - corner_point)
    end

    local edge_a_point = find_most_opposite_edge(edges_from_corner[1], edges_from_corner)
    local edge_b_point = find_most_opposite_edge(edge_a_point, edges_from_corner)

    local normal_sum = sk_math.vector3(0, 0, 0)

    for _, normal in pairs(mesh.normals) do
        normal_sum = normal_sum + normal
    end

    local final_normal = normal_sum:normalized()

    -- make sure the basis is right handed
    if edge_a_point:cross(edge_b_point):dot(final_normal) < 0 then
        edge_a_point, edge_b_point = edge_b_point, edge_a_point
    end

    local edge_a_normalized = edge_a_point:normalized()
    local edge_b_normalized = edge_b_point:normalized()


    return {
        corner = corner_point,
        edgeA = edge_a_normalized,
        edgeALength = edge_a_point:dot(edge_a_normalized),
        edgeB = edge_b_normalized,
        edgeBLength = edge_b_point:dot(edge_b_normalized),
        plane = {
            normal = final_normal,
            d = -corner_point:dot(final_normal),
        },
        bb = mesh.bb,
    }
end

local colliders = {}

for index, node in pairs(sk_scene.nodes_for_type("@collision")) do        
    for _, mesh in pairs(node.node.meshes) do
        local global_mesh = mesh:transform(node.node.full_transformation)
        table.insert(colliders, create_collision_quad(global_mesh))
    end
end

sk_definition_writer.add_definition("quad_colliders", "struct CollisionQuad[]", "_geo", colliders)

return {
    colliders = colliders,
    collision_quad_bb = collision_quad_bb,
}