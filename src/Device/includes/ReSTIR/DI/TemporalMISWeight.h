/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef DEVICE_RESTIR_DI_MIS_WEIGHT_H
#define DEVICE_RESTIR_DI_MIS_WEIGHT_H

#include "Device/includes/ReSTIR/DI/Utils.h"
#include "HostDeviceCommon/KernelOptions.h"

 // By convention, the temporal neighbor is the first one to be resampled in for loops 
 // (for looping over the neighbors when resampling / computing MIS weights)
 // So instead of hardcoding 0 everywhere in the code, we just basically give it a name
 // with a #define
#define TEMPORAL_NEIGHBOR_ID 0
// Same when resampling the initial candidates
#define INITIAL_CANDIDATES_ID 1

/**
 * This structure here is only meant to encapsulate one method that
 * returns the resampling MIS weight used by the temporal resampling pass.
 * 
 * This whole file basically defines the functions to compute the different resampling
 * MIS weights that the renderer supports.
 * 
 * This is cleaner that having a single function with a ton of 
 * 
 * #if BiasCorrectionmode == 1_OVER_M
 * #elif BiasCorrectionmode == 1_OVER_Z
 * #elif BiasCorrectionmode == MIS_LIKE
 * ....
 * 
 * We now have one structure per MIS weight computation mode instead of one #if / #elif
 */
template <int BiasCorrectionMode>
struct ReSTIRDITemporalResamplingMISWeight {};

template<>
struct ReSTIRDITemporalResamplingMISWeight<RESTIR_DI_BIAS_CORRECTION_1_OVER_M>
{
	HIPRT_HOST_DEVICE float get_resampling_MIS_weight(const HIPRTRenderData& render_data,
		const ReSTIRDIReservoir& reservoir_being_resampled,
		const ReSTIRDISurface& temporal_neighbor_surface, const ReSTIRDISurface& center_pixel_surface,
		int initial_candidates_reservoir_M, int temporal_neighbor_reservoir_M,
		int current_neighbor,
		Xorshift32Generator& random_number_generator)
	{
		// 1/M MIS Weights are basically confidence weights only so we only need to return
		// the confidence of the reservoir

		return reservoir_being_resampled.M;
	}
};

template<>
struct ReSTIRDITemporalResamplingMISWeight<RESTIR_DI_BIAS_CORRECTION_1_OVER_Z>
{
	HIPRT_HOST_DEVICE float get_resampling_MIS_weight(const HIPRTRenderData& render_data,
		const ReSTIRDIReservoir& reservoir_being_resampled,
		const ReSTIRDISurface& temporal_neighbor_surface, const ReSTIRDISurface& center_pixel_surface,
		int initial_candidates_reservoir_M, int temporal_neighbor_reservoir_M,
		int current_neighbor,
		Xorshift32Generator& random_number_generator)
	{
		// 1/Z MIS Weights are basically confidence weights only so we only need to return
		// the confidence of the reservoir. The difference with 1/M weights is how we're going
		// to normalize the reservoir at the end of the temporal/spatial resampling pass

		return reservoir_being_resampled.M;
	}
};





template<>
struct ReSTIRDITemporalResamplingMISWeight<RESTIR_DI_BIAS_CORRECTION_MIS_LIKE>
{
	HIPRT_HOST_DEVICE float get_resampling_MIS_weight(const HIPRTRenderData& render_data,
		const ReSTIRDIReservoir& reservoir_being_resampled,
		const ReSTIRDISurface& temporal_neighbor_surface, const ReSTIRDISurface& center_pixel_surface,
		int initial_candidates_reservoir_M, int temporal_neighbor_reservoir_M,
		int current_neighbor,
		Xorshift32Generator& random_number_generator)
	{
		// MIS-like MIS weights without confidence weights do not weight the neighbor reservoirs
		// during resampling (the same goes with any MIS weights that doesn't use confidence
		// weights). We're thus returning 1.0f.
		// 
		// The bulk of the work of the MIS-like weights is done in during the normalization of the reservoir

		return 1.0f;
	}
};

template<>
struct ReSTIRDITemporalResamplingMISWeight<RESTIR_DI_BIAS_CORRECTION_MIS_LIKE_CONFIDENCE_WEIGHTS>
{
	HIPRT_HOST_DEVICE float get_resampling_MIS_weight(const HIPRTRenderData& render_data,
		const ReSTIRDIReservoir& reservoir_being_resampled, const ReSTIRDIReservoir& temporal_neighbor_reservoir,
		const ReSTIRDISurface& temporal_neighbor_surface, const ReSTIRDISurface& center_pixel_surface,
		int current_neighbor,
		Xorshift32Generator& random_number_generator)
	{
		// MIS-like MIS weights with confidence weights are basically a mix of 1/Z 
		// and MIS like for the normalization so we're just returning the confidence here
		// so that a reservoir that is being resampled gets a bigger weight depending on its 
		// confidence weight (M).

		return reservoir_being_resampled.M;
	}
};





template<>
struct ReSTIRDITemporalResamplingMISWeight<RESTIR_DI_BIAS_CORRECTION_MIS_GBH>
{
	HIPRT_HOST_DEVICE float get_resampling_MIS_weight(const HIPRTRenderData& render_data,
		const ReSTIRDIReservoir& reservoir_being_resampled,
		const ReSTIRDISurface& temporal_neighbor_surface, const ReSTIRDISurface& center_pixel_surface,
		int initial_candidates_reservoir_M, int temporal_neighbor_reservoir_M,
		int current_neighbor,
		Xorshift32Generator& random_number_generator)
	{
		float nume = 0.0f;
		// We already have the target function at the center pixel, adding it to the denom
		float denom = 0.0f;

		// Evaluating the sample that we're resampling at the neighor locations (using the neighbors surfaces)
		float target_function_at_temporal_neighbor = 0.0f;
		if (temporal_neighbor_reservoir_M != 0)
			// Only computing the target function if we do have a temporal neighbor
			target_function_at_temporal_neighbor = ReSTIR_DI_evaluate_target_function<ReSTIR_DI_BiasCorrectionUseVisiblity>(render_data, reservoir_being_resampled.sample, temporal_neighbor_surface);

		// TODO compute visibility before in target function

		if (current_neighbor == TEMPORAL_NEIGHBOR_ID && target_function_at_temporal_neighbor == 0.0f)
			// If we're currently computing the MIS weight for the temporal neighbor,
			// this means that we're going to have the temporal neighbor weight 
			// (target function) in the numerator. But if the target function
			// at the temporal neighbor is 0.0f, then we're going to have 0.0f
			// in the numerator --> 0.0f MIS weight anyways --> no need to
			// compute anything else, we can already return 0.0f for the MIS weight.
			return 0.0f;

		float target_function_at_center = ReSTIR_DI_evaluate_target_function<ReSTIR_DI_BiasCorrectionUseVisiblity>(render_data, reservoir_being_resampled.sample, center_pixel_surface);

		denom = target_function_at_temporal_neighbor + target_function_at_center;
		if (current_neighbor == TEMPORAL_NEIGHBOR_ID)
			nume = target_function_at_temporal_neighbor;
		else
			nume = target_function_at_center;

		if (denom == 0.0f)
			return 0.0f;
		else
			return nume / denom;
	}
};

template<>
struct ReSTIRDITemporalResamplingMISWeight<RESTIR_DI_BIAS_CORRECTION_MIS_GBH_CONFIDENCE_WEIGHTS>
{
	HIPRT_HOST_DEVICE float get_resampling_MIS_weight(const HIPRTRenderData& render_data,
		const ReSTIRDIReservoir& reservoir_being_resampled,
		const ReSTIRDISurface& temporal_neighbor_surface, const ReSTIRDISurface& center_pixel_surface,
		int initial_candidates_reservoir_M, int temporal_neighbor_reservoir_M,
		int current_neighbor,
		Xorshift32Generator& random_number_generator)
	{
		float nume = 0.0f;
		// We already have the target function at the center pixel, adding it to the denom
		float denom = 0.0f;

		// Evaluating the sample that we're resampling at the neighor locations (using the neighbors surfaces)
		float target_function_at_temporal_neighbor = 0.0f;
		if (temporal_neighbor_reservoir_M != 0)
			// Only computing the target function if we do have a temporal neighbor
			target_function_at_temporal_neighbor = ReSTIR_DI_evaluate_target_function<ReSTIR_DI_BiasCorrectionUseVisiblity>(render_data, reservoir_being_resampled.sample, temporal_neighbor_surface);

		// TODO compute visibility before in target function

		if (current_neighbor == TEMPORAL_NEIGHBOR_ID && target_function_at_temporal_neighbor == 0.0f)
			// If we're currently computing the MIS weight for the temporal neighbor,
			// this means that we're going to have the temporal neighbor weight 
			// (target function) in the numerator. But if the target function
			// at the temporal neighbor is 0.0f, then we're going to have 0.0f
			// in the numerator --> 0.0f MIS weight anyways --> no need to
			// compute anything else, we can already return 0.0f for the MIS weight.
			return 0.0f;

		float target_function_at_center = ReSTIR_DI_evaluate_target_function<ReSTIR_DI_BiasCorrectionUseVisiblity>(render_data, reservoir_being_resampled.sample, center_pixel_surface);

		denom = target_function_at_temporal_neighbor * temporal_neighbor_reservoir_M + target_function_at_center * initial_candidates_reservoir_M;
		if (current_neighbor == TEMPORAL_NEIGHBOR_ID)
			nume = target_function_at_temporal_neighbor * temporal_neighbor_reservoir_M;
		else
			nume = target_function_at_center * initial_candidates_reservoir_M;

		if (denom == 0.0f)
			return 0.0f;
		else
			return nume / denom;
	}
};

#endif
