/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef GPU_RENDERER_H
#define GPU_RENDERER_H

#include "HIPRT-Orochi/OrochiBuffer.h"
#include "HIPRT-Orochi/OrochiEnvmap.h"
#include "HIPRT-Orochi/HIPRTOrochiCtx.h"
#include "HIPRT-Orochi/HIPRTScene.h"
#include "HostDeviceCommon/RenderData.h"
#include "OpenGL/OpenGLInteropBuffer.h"
#include "Renderer/OpenImageDenoiser.h"
#include "Scene/Camera.h"
#include "Scene/SceneParser.h"

#include <vector>

/*
 * Temporary workaround to disable not-yet-supported OpenGL interoperability
 * on NVIDIA hardware. Falling back to the classic OrochiBuffer that goes
 * through the CPU before OpenGL
 */
#ifdef OROCHI_ENABLE_CUEW
template <typename T>
using InteropBufferType = OrochiBuffer<T>;
#else
template <typename T>
using InteropBufferType = OpenGLInteropBuffer<T>;
#endif

class GPURenderer
{
public:

	GPURenderer(int width, int height, HIPRTOrochiCtx* hiprt_orochi_ctx) : 
		m_render_width(width), m_render_height(height), m_hiprt_orochi_ctx(hiprt_orochi_ctx),
		m_trace_kernel(nullptr)
	{
		m_hiprt_scene.hiprt_ctx = hiprt_orochi_ctx->hiprt_ctx;
	}

	GPURenderer() {}

	void render();
	void change_render_resolution(int new_width, int new_height);

	InteropBufferType<ColorRGB>& get_color_framebuffer();
	OrochiBuffer<ColorRGB>& get_denoiser_albedo_buffer();
	OrochiBuffer<hiprtFloat3>& get_denoiser_normals_buffer();
	OrochiBuffer<int>& get_pixels_sample_count_buffer();

	OrochiBuffer<int>& get_debug_pixel_active_buffer();

	HIPRTRenderSettings& get_render_settings();
	WorldSettings& get_world_settings();
	HIPRTRenderData get_render_data();

	void init_ctx(int device_index);
	void compile_trace_kernel(const char* kernel_file_path, const char* kernel_function_name);
	void launch_kernel(int tile_size_x, int tile_size_y, int res_x, int res_y, void** launch_args);

	void set_scene(Scene& scene);
	void set_envmap(OrochiEnvmap& envmap);
	void set_camera(const Camera& camera);

	const std::vector<RendererMaterial>& get_materials();
	void update_materials(std::vector<RendererMaterial>& materials);

	void translate_camera_view(glm::vec3 translation);
	void rotate_camera_view(glm::vec3 rotation_angles);
	void zoom_camera_view(float offset);

	int get_sample_number();
	void set_sample_number(int sample_numner);

	int m_render_width = 0, m_render_height = 0;

	Camera m_camera;

private:
	void set_hiprt_scene_from_scene(Scene& scene);

	OrochiBuffer<int> m_debug_pixel_active;

	// This buffer holds the * sum * of the samples computed
	// This is an accumulation buffer. This needs to be divided by the
	// number of samples for displaying
	InteropBufferType<ColorRGB> m_pixels_interop_buffer;
	// Normals G-buffer
	OrochiBuffer<hiprtFloat3> m_normals_buffer;
	// Albedo G-buffer
	OrochiBuffer<ColorRGB> m_albedo_buffer;

	// Used to calculate the variance of each pixel for adaptative sampling
	OrochiBuffer<float> m_pixels_squared_luminance;
	// This buffer is necessary because with adaptative sampling, each pixel
	// can have accumulated a different number of sample
	OrochiBuffer<int> m_pixels_sample_count;

	OrochiEnvmap m_envmap;

	std::shared_ptr<HIPRTOrochiCtx> m_hiprt_orochi_ctx;
	oroFunction m_trace_kernel = nullptr;

	HIPRTScene m_hiprt_scene = nullptr;
	// The materials are also kept on the CPU side because we want to be able
	// to modify them interactively with ImGui
	std::vector<RendererMaterial> m_materials;

	WorldSettings m_world_settings;
	HIPRTRenderSettings m_render_settings;
};

#endif
