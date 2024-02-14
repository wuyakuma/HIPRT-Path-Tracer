#ifndef SCENE_PARSER_H
#define SCENE_PARSER_H

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "hiprt/hiprt_vec.h"

#include "Scene/camera.h"
#include "Renderer/renderer_material.h"
#include "Renderer/sphere.h"
#include "Renderer/triangle.h"

#include <vector>

struct Scene
{
    std::vector<RendererMaterial> materials;

    std::vector<int> vertices_indices;
    std::vector<hiprtFloat3> vertices_positions;
    std::vector<int> emissive_triangle_indices;
    std::vector<int> material_indices;

    bool has_camera = false;
    Camera camera;

    Sphere add_sphere(const Point& center, float radius, const RendererMaterial& material, int primitive_index)
    {
        int material_index = materials.size();

        materials.push_back(material);
        material_indices.push_back(material_index);

        Sphere sphere(center, radius, primitive_index);

        return sphere;
    }

    std::vector<Triangle> get_triangles()
    {
        std::vector<Triangle> triangles;

        for (int i = 0; i < vertices_indices.size(); i += 3)
        {
            triangles.push_back(Triangle(*reinterpret_cast<Point*>(&vertices_positions[vertices_indices[i + 0]]),
                                         *reinterpret_cast<Point*>(&vertices_positions[vertices_indices[i + 1]]),
                                         *reinterpret_cast<Point*>(&vertices_positions[vertices_indices[i + 2]])));
        }

        return triangles;
    }
};

class SceneParser
{
public:
    static RendererMaterial ai_mat_to_renderer_mat(aiMaterial* mesh_material);

    /**
     * Parses the scene file at @filepath and returns a scene appropriate for the renderer.
     * All formats supported by the ASSIMP library are supported by the renderer
     * 
     * If provided, the @frame_aspect_override parameter is meant to override the aspect ratio of the camera
     * of the scene file (if any). This is useful because the renderer uses a default aspect ratio
     * of 16:9 but the camera of the scene file ma not use the same aspect. Without this parameter,
     * this would result in rendering the scene with an aspect different of 16:9 in the defualt 
     * framebuffer of the renderer which is 16:9, resulting in deformations.
     */
    static Scene parse_scene_file(const std::string& filepath, float frame_aspect_override = -1.0f);
};

#endif
