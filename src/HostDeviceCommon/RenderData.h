/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */


#ifndef HOST_DEVICE_COMMON_RENDER_DATA_H
#define HOST_DEVICE_COMMON_RENDER_DATA_H

#include "HostDeviceCommon/Material.h"
#include "HostDeviceCommon/Math.h"

#include <hiprt/hiprt_device.h>
#include <Orochi/Orochi.h>

#ifdef __KERNELCC__
template <typename T>
using AtomicType = T;
#else
#include <atomic>

template <typename T>
using AtomicType = std::atomic<T>;
#endif

struct HIPRTRenderSettings
{
	// How many times the render kernel was called (updates after
	// the call to the kernel so it start at 0)
	int frame_number = 0;

	// Number of samples rendered so far before the kernel call
	// This is the sum of samples_per_frame for all frames
	// that have been rendered.
	int sample_number = 0;

	int samples_per_frame = 1;
	int nb_bounces = 4;

	// Whether or not to "freeze" random number generation so that each frame uses
	// exactly the same random number. This allows every ray to follow the exact
	// same path every frame, allowing for more stable benchmarking.
	int freeze_random = false;

	// If true, NaNs encountered during rendering will be rendered as very bright pink. 
	// Useful for debugging only.
	bool display_NaNs = false;

	// If true, this means that the user is moving the camera and we're going to
	// render the image at a much lower resolution to allow for smooth camera
	// movements
	int render_low_resolution = false;
	// How to divide the render resolution by when rendering at low resolution
	// (when interacting with the camera)
	int render_low_resolution_scaling = 4;

	int enable_adaptive_sampling = true;
	// How many samples before the adaptive sampling actually kicks in.
	// This is useful mainly for the per-pixel adaptive sampling method
	// where you want to be sure that each pixel in the image has had enough
	// chance find a path to a potentially 
	int adaptive_sampling_min_samples = 96;
	// Adaptive sampling noise threshold
	float adaptive_sampling_noise_threshold = 0.3f;

	// A percentage in [0, 100] that dictates the proportion of pixels that must
	// have reached the given noise threshold (stop_pixel_noise_threshold
	// variable) before we stop rendering.
	// For example, if this variable is 90, we will stop rendering when 90% of all
	// pixels have reached the stop_pixel_noise_threshold
	float stop_pixel_percentage_converged = 90.0f;
	// Noise threshold for use with the stop_pixel_percentage_converged stopping
	// condition
	float stop_pixel_noise_threshold = 0.0f;



	// Clamp direct lighting contribution to reduce fireflies
	float direct_contribution_clamp = 0.0f;
	// Clamp envmap contribution to reduce fireflies
	float envmap_contribution_clamp = 0.0f;
	// Clamp indirect lighting contribution to reduce fireflies
	float indirect_contribution_clamp = 0.0f;

	// How many candidate lights to sample for RIS (Resampled Importance Sampling)
	int ris_number_of_light_candidates = 8;
	// How many candidates samples from the BSDF to use in combination
	// with the light candidates for RIS
	int ris_number_of_bsdf_candidates = 1;



	/**
	 * Returns true if the adaptive sampling buffers are ready for use, false otherwise.
	 * 
	 * Adaptive sampling buffers are "ready for use" if the adaptive sampling is enabled or
	 * if the pixel stop noise threshold is enabled. Otherwise, the adaptive sampling buffers
	 * are freed to save VRAM so they cannot be used.
	 */
	HIPRT_HOST_DEVICE bool has_access_to_adaptive_sampling_buffers()
	{
		bool has_access = false;

		has_access |= stop_pixel_noise_threshold > 0.0f; 
		has_access |= enable_adaptive_sampling == 1;

		return has_access;
	}
};

struct RenderBuffers
{
	// Sum of samples color per pixel. Should not be
	// pre-divided by the number of samples
	ColorRGB32F* pixels = nullptr;

	// A device pointer to the buffer of triangles vertex indices
	// triangles_indices[0], triangles_indices[1] and triangles_indices[2]
	// represent the indices of the vertices of the first triangle for example
	int* triangles_indices = nullptr;
	// A device pointer to the buffer of triangle vertices positions
	float3* vertices_positions = nullptr;
	// A device pointer to a buffer filled with 0s and 1s that
	// indicates whether or not a vertex normal is available for
	// the given vertex index
	unsigned char* has_vertex_normals = nullptr;
	// The smooth normal at each vertex of the scene
	// Needs to be indexed by a vertex index
	float3* vertex_normals = nullptr;
	// Texture coordinates at each vertices
	float2* texcoords = nullptr;

	// Index of the material used by each triangle of the scene
	int* material_indices = nullptr;
	// Materials array to be indexed by an index retrieved from the 
	// material_indices array
	RendererMaterial* materials_buffer = nullptr;
	int emissive_triangles_count = 0;
	int* emissive_triangles_indices = nullptr;

	// A pointer either to an array of Image8Bit or to an array of
	// oroTextureObject_t whether if CPU or GPU rendering respectively
	// This pointer can be cast for the textures to be be retrieved.
	void* material_textures = nullptr;
	// Widths of the textures. Necessary for using texel coordinates in [0, width - 1]
	// in the shader (required because Orochi doesn't support normalized texture coordinates).
	int2* textures_dims = nullptr;
};

struct AuxiliaryBuffers
{
	// World space normals for the denoiser
	// These normals should already be divided by the number of samples
	float3* denoiser_normals = nullptr;

	// Albedo for the denoiser
	// The albedo should already be divided by the number of samples
	ColorRGB32F* denoiser_albedo = nullptr;

	// Per pixel sample count. Useful when doing adaptive sampling
	// where each pixel can have a different number of sample
	int* pixel_sample_count = nullptr;

	// Per pixel sum of squared luminance of samples. Used for adaptive sampling
	// This buffer should not be pre-divided by the number of samples
	float* pixel_squared_luminance = nullptr;

	// A single boolean (contained in a buffer, hence the pointer) 
	// to indicate whether at least one single ray is still active in the kernel.
	// This is an unsigned char instead of a boolean because std::vector<bool>.data()
	// isn't standard
	unsigned char* still_one_ray_active = nullptr;

	// If render_settings.stop_pixel_noise_threshold > 0.0f, this buffer
	// (consisting of a single unsigned int) counts how many pixels have reached the
	// noise threshold. If this value is equal to the number of pixels of the
	// framebuffer, then all pixels have converged according to the given
	// noise threshold.
	AtomicType<unsigned int>* stop_noise_threshold_count = nullptr;
};

enum AmbientLightType
{
	NONE,
	UNIFORM,
	ENVMAP
};

struct WorldSettings
{
	AmbientLightType ambient_light_type = AmbientLightType::ENVMAP;
	ColorRGB32F uniform_light_color = ColorRGB32F(0.5f);

	// Width and height in pixels. Both in the range [1, XXX]
	unsigned int envmap_width = 0, envmap_height = 0;
	// Simple scale multiplier on the envmap color read from the envmap texture
	// in the shader
	float envmap_intensity = 1.0f;
	// If true, the background of the scene (where rays directly miss any geometry
	// and we directly see the skysphere) will scale with the envmap_intensity coefficient.
	// This can be visually unpleasing because the background will most likely
	// become completely white and blown out.
	int envmap_scale_background_intensity = false;
	// This void pointer is a either a float* for the CPU
	// or a oroTextureObject_t for the GPU.
	// Proper reinterpreting of the pointer is done in the kernel.
	void* envmap = nullptr;
	// Cumulative distribution function. 1D float array of length width * height for
	// importance sampling the envmap
	float* envmap_cdf = nullptr;
	// Rotation matrix for rotating the envmap around
	float4x4 envmap_rotation_matrix = float4x4{ { {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f } } };
};

/**
 * The CPU and GPU use the same kernel code but the CPU still need some specific data
 * (the CPU BVH for example) which is stored in this structure
 */

class BVH;
struct CPUData
{
	BVH* bvh = nullptr;
};

/*
 * A structure containing all the information about the scene
 * that the kernel is going to need for the render (vertices of the triangles, 
 * vertices indices, skysphere data, ...)
 */
struct HIPRTRenderData
{
	hiprtGeometry geom = nullptr;

	RenderBuffers buffers;
	AuxiliaryBuffers aux_buffers;
	WorldSettings world_settings;

	HIPRTRenderSettings render_settings;

	CPUData cpu_only;
};

#endif
