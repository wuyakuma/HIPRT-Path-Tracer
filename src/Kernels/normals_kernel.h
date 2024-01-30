#include "Kernels/includes/HIPRT_maths.h"
#include "Kernels/includes/HIPRT_common.h"

#include <hiprt/hiprt_device.h>
#include <hiprt/hiprt_vec.h>


GLOBAL_KERNEL_SIGNATURE(void) NormalsKernel(hiprtGeometry geom, HIPRTSceneData scene_geometry, float* pixels, int2 res, Camera camera)
{
	const uint32_t x = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t y = blockIdx.y * blockDim.y + threadIdx.y;
	const uint32_t index = x + y * res.x;

	hiprtRay ray = get_camera_ray(camera, x, y, res.x, res.y);

	hiprtGeomTraversalClosest tr(geom, ray);
	hiprtHit				  hit = tr.getNextHit();

	hiprtFloat3 normal = normalize(hit.normal);
	int index_A = scene_geometry.triangles_indices[hit.primID * 3 + 0];
	int index_B = scene_geometry.triangles_indices[hit.primID * 3 + 1];
	int index_C = scene_geometry.triangles_indices[hit.primID * 3 + 2];

	hiprtFloat3 vertex_A = scene_geometry.triangles_vertices[index_A];
	hiprtFloat3 vertex_B = scene_geometry.triangles_vertices[index_B];
	hiprtFloat3 vertex_C = scene_geometry.triangles_vertices[index_C];

	normal = hiprtFloat3{ 0.5f, 0.5f, 0.5f } * normalize(cross(vertex_B - vertex_A, vertex_C - vertex_A)) + hiprtFloat3{ 0.5f, 0.5f, 0.5f };

	pixels[index * 4 + 0] = hit.hasHit() ? normal.x : 0.0f;
	pixels[index * 4 + 1] = hit.hasHit() ? normal.y : 0.0f;
	pixels[index * 4 + 2] = hit.hasHit() ? normal.z : 0.0f;
	pixels[index * 4 + 3] = 1.0f;
}