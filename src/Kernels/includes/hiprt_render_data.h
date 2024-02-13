#ifndef HIPRT_SCENE_DATA_H
#define HIPRT_SCENE_DATA_H

#include "Kernels/includes/HIPRT_common.h"

/*
 * A structure containing all the information about the scene
 * that the kernel is going to need for the render (vertices of the triangles, 
 * vertices indices, skysphere data, ...)
 */
struct HIPRTRenderData
{
	HIPRTRenderData() : triangles_indices(nullptr), triangles_vertices(nullptr), frame_number(0), nb_bounces(8) {}

	// A device pointer to the buffer of triangles indices
	int* triangles_indices;
	// A device pointer to the buffer of triangle vertices
	hiprtFloat3* triangles_vertices;
	// Index of the material used by each triangle of the scene
	int* material_indices;
	// Materials array to be indexed by an index retrieved from the 
	// material_indices array
	HIPRTRendererMaterial* materials_buffer;

	// Starts at 0
	int frame_number;
	int nb_bounces;
};

#endif