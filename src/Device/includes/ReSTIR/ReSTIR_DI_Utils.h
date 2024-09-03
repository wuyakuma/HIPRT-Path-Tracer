/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef DEVICE_RESTIR_DI_UTILS_H
#define DEVICE_RESTIR_DI_UTILS_H 

#include "Device/includes/Dispatcher.h"
#include "Device/includes/Intersect.h"
#include "Device/includes/LightUtils.h"
#include "Device/includes/ReSTIR/ReSTIR_DI_Surface.h"

#include "HostDeviceCommon/RenderData.h"

template <bool withVisiblity>
HIPRT_HOST_DEVICE HIPRT_INLINE float ReSTIR_DI_evaluate_target_function(const HIPRTRenderData& render_data, const ReSTIRDISample& sample, const ReSTIRDISurface& surface)
{
#ifndef __KERNELCC__
	std::cerr << "ReSTIR_DI_evaluate_target_function() wrong specialization called: " << withVisiblity << std::endl;
	Utils::debugbreak();
#endif

	return -1.0f;
}

template <>
HIPRT_HOST_DEVICE HIPRT_INLINE float ReSTIR_DI_evaluate_target_function<KERNEL_OPTION_FALSE>(const HIPRTRenderData& render_data, const ReSTIRDISample& sample, const ReSTIRDISurface& surface)
{
	if (sample.emissive_triangle_index == -1)
		// No sample
		return 0.0f;

	float bsdf_pdf;
	float distance_to_light;
	float3 sample_direction;
	sample_direction = sample.point_on_light_source - surface.shading_point;
	sample_direction = sample_direction / (distance_to_light = hippt::length(sample_direction));

	RayVolumeState trash_volume_state = surface.ray_volume_state;
	ColorRGB32F bsdf_color = bsdf_dispatcher_eval(render_data.buffers.materials_buffer, surface.material, trash_volume_state, surface.view_direction, surface.shading_normal, sample_direction, bsdf_pdf);
	float cosine_term = hippt::max(0.0f, hippt::dot(surface.shading_normal, sample_direction));

	float geometry_term = 1.0f;
	if (render_data.render_settings.restir_di_settings.target_function.geometry_term_in_target_function)
	{
		float3 light_source_normal = hippt::normalize(get_triangle_normal_non_normalized(render_data, sample.emissive_triangle_index));
		float cosine_at_light_source = hippt::abs(hippt::dot(sample_direction, light_source_normal));

		geometry_term = cosine_at_light_source / (distance_to_light * distance_to_light);
	}

	int material_index = render_data.buffers.material_indices[sample.emissive_triangle_index];
	ColorRGB32F sample_emission = render_data.buffers.materials_buffer[material_index].emission;

	float target_function = (bsdf_color * sample_emission * cosine_term * geometry_term).luminance();
	if (target_function == 0.0f)
		// Quick exit because computing the visiblity that follows isn't going
		// to change anything to the fact that we have 0.0f target function here
		return 0.0f;

	return target_function;
}

template <>
HIPRT_HOST_DEVICE HIPRT_INLINE float ReSTIR_DI_evaluate_target_function<KERNEL_OPTION_TRUE>(const HIPRTRenderData& render_data, const ReSTIRDISample& sample, const ReSTIRDISurface& surface)
{
	if (sample.emissive_triangle_index == -1)
		// No sample
		return 0.0f;

	float bsdf_pdf;
	float distance_to_light;
	float3 sample_direction;
	sample_direction = sample.point_on_light_source - surface.shading_point;
	sample_direction = sample_direction / (distance_to_light = hippt::length(sample_direction));

	RayVolumeState trash_volume_state = surface.ray_volume_state;
	ColorRGB32F bsdf_color = bsdf_dispatcher_eval(render_data.buffers.materials_buffer, surface.material, trash_volume_state, surface.view_direction, surface.shading_normal, sample_direction, bsdf_pdf);
	float cosine_term = hippt::max(0.0f, hippt::dot(surface.shading_normal, sample_direction));

	float geometry_term = 1.0f;
	if (render_data.render_settings.restir_di_settings.target_function.geometry_term_in_target_function)
	{
		float3 light_source_normal = hippt::normalize(get_triangle_normal_non_normalized(render_data, sample.emissive_triangle_index));
		float cosine_at_light_source = hippt::abs(hippt::dot(sample_direction, light_source_normal));

		geometry_term = cosine_at_light_source / (distance_to_light * distance_to_light);
	}

	int material_index = render_data.buffers.material_indices[sample.emissive_triangle_index];
	ColorRGB32F sample_emission = render_data.buffers.materials_buffer[material_index].emission;

	float target_function = (bsdf_color * sample_emission * cosine_term * geometry_term).luminance();
	if (target_function == 0.0f)
		// Quick exit because computing the visiblity that follows isn't going
		// to change anything to the fact that we have 0.0f target function here
		return 0.0f;

	hiprtRay shadow_ray;
	shadow_ray.origin = surface.shading_point;
	shadow_ray.direction = sample_direction;

	bool visible = !evaluate_shadow_ray(render_data, shadow_ray, distance_to_light);

	target_function *= visible;

	return target_function;
}

HIPRT_HOST_DEVICE HIPRT_INLINE float get_jacobian_determinant_reconnection_shift(const HIPRTRenderData& render_data, const ReSTIRDIReservoir& neighbor_reservoir, const float3& center_pixel_shading_point, const float3& neighbor_shading_point)
{
	float distance_to_light_at_center;
	float distance_to_light_at_neighbor;
	float3 to_light_direction_at_center = neighbor_reservoir.sample.point_on_light_source - center_pixel_shading_point;
	float3 to_light_direction_at_neighbor = neighbor_reservoir.sample.point_on_light_source - neighbor_shading_point;
	to_light_direction_at_center /= (distance_to_light_at_center = hippt::length(to_light_direction_at_center));
	to_light_direction_at_neighbor /= (distance_to_light_at_neighbor = hippt::length(to_light_direction_at_neighbor));

	float3 light_source_normal = hippt::normalize(get_triangle_normal_non_normalized(render_data, neighbor_reservoir.sample.emissive_triangle_index));

	float cosine_light_source_at_center = hippt::abs(hippt::dot(-to_light_direction_at_center, light_source_normal));
	float cosine_light_source_at_neighbor = hippt::abs(hippt::dot(-to_light_direction_at_neighbor, light_source_normal));

	float cosine_ratio = cosine_light_source_at_center / cosine_light_source_at_neighbor;
	float distance_squared_ratio = (distance_to_light_at_neighbor * distance_to_light_at_neighbor) / (distance_to_light_at_center * distance_to_light_at_center);

	float jacobian = cosine_ratio * distance_squared_ratio;

	float jacobian_clamp = 20.0f;
	if (jacobian > jacobian_clamp || jacobian < 1.0f / jacobian_clamp || hippt::isNaN(jacobian))
		// Samples are too dissimilar, returning -1 to indicate that we must reject the sample
		return -1;
	else
		return jacobian;
}

HIPRT_HOST_DEVICE HIPRT_INLINE float get_jacobian_determinant_reconnection_shift(const HIPRTRenderData& render_data, const ReSTIRDIReservoir& neighbor_reservoir, const float3& center_pixel_shading_point, int neighbor_pixel_index)
{
	return get_jacobian_determinant_reconnection_shift(render_data, neighbor_reservoir, center_pixel_shading_point, render_data.g_buffer.first_hits[neighbor_pixel_index]);
}

/**
 * Returns true if the two given points pass the plane distance check, false otherwise
 */
HIPRT_HOST_DEVICE HIPRT_INLINE bool plane_distance_heuristic(const float3& temporal_world_space_point, const float3& current_point, const float3& current_surface_normal, float plane_distance_threshold)
{
	float3 direction_between_points = temporal_world_space_point - current_point;
	float distance_to_plane = hippt::abs(hippt::dot(direction_between_points, current_surface_normal));

	return distance_to_plane < plane_distance_threshold;
}

HIPRT_HOST_DEVICE HIPRT_INLINE bool normal_similarity_heuristic(const float3& current_normal, const float3& neighbor_normal, float threshold)
{
	return hippt::dot(current_normal, neighbor_normal) > threshold;
}

HIPRT_HOST_DEVICE HIPRT_INLINE bool roughness_similarity_heuristic(float neighbor_roughness, float center_pixel_roughness, float threshold)
{
	// We don't want to temporally reuse on materials smoother than 0.075f because this
	// causes near-specular/glossy reflections to darken when camera ray jittering is used.
	// 
	// This glossy reflections darkening only happens with confidence weights and 
	// ray jittering but I'm not sure why. Probably because samples from one pixel (or sub-pixel location)
	// cannot efficiently be reused at another pixel (or sub-pixel location through jittering)
	// but confidence weights overweight these bad neighbor samples --> you end up using these
	// bad samples --> the shading loses in energy since we're now shading with samples that
	// don't align well with the glossy reflection direction
	return hippt::abs(neighbor_roughness - center_pixel_roughness) < threshold;
}

HIPRT_HOST_DEVICE HIPRT_INLINE bool check_similarity_heuristics(const HIPRTRenderData& render_data, int neighbor_index, int center_pixel_index, const float3& current_shading_point, const float3& current_normal)
{
	float3 temporal_neighbor_point = render_data.g_buffer.first_hits[neighbor_index];

	float temporal_neighbor_roughness = render_data.g_buffer.materials[neighbor_index].roughness;
	float current_material_roughness = render_data.g_buffer.materials[center_pixel_index].roughness;

	bool plane_distance_passed = plane_distance_heuristic(temporal_neighbor_point, current_shading_point, current_normal, render_data.render_settings.restir_di_settings.plane_distance_threshold);
	bool normal_similarity_passed = normal_similarity_heuristic(current_normal, render_data.g_buffer.shading_normals[neighbor_index], render_data.render_settings.restir_di_settings.normal_similarity_angle_precomp);
	bool roughness_similarity_passed = roughness_similarity_heuristic(temporal_neighbor_roughness, current_material_roughness, render_data.render_settings.restir_di_settings.roughness_similarity_threshold);

	return plane_distance_passed && normal_similarity_passed && roughness_similarity_passed;
}

#endif