/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#include "Compiler/GPUKernelCompiler.h"
#include "Compiler/GPUKernel.h"
#include "Compiler/GPUKernelCompilerOptions.h"
#include "HIPRT-Orochi/HIPRTOrochiUtils.h"
#include "Threads/ThreadFunctions.h"
#include "Threads/ThreadManager.h"
#include "UI/ImGui/ImGuiLogger.h"

extern GPUKernelCompiler g_gpu_kernel_compiler;
extern ImGuiLogger g_imgui_logger;

const std::vector<std::string> GPUKernel::COMMON_ADDITIONAL_KERNEL_INCLUDE_DIRS =
{
	KERNEL_COMPILER_ADDITIONAL_INCLUDE,
	DEVICE_INCLUDES_DIRECTORY,
	OROCHI_INCLUDES_DIRECTORY,
	"./"
};

GPUKernel::GPUKernel()
{
	OROCHI_CHECK_ERROR(oroEventCreate(&m_execution_start_event));
	OROCHI_CHECK_ERROR(oroEventCreate(&m_execution_stop_event));
}

GPUKernel::GPUKernel(const std::string& kernel_file_path, const std::string& kernel_function_name) : GPUKernel()
{
	m_kernel_file_path = kernel_file_path;
	m_kernel_function_name = kernel_function_name;
}

std::string GPUKernel::get_kernel_file_path() const
{
	return m_kernel_file_path;
}

std::string GPUKernel::get_kernel_function_name() const
{
	return m_kernel_function_name;
}

void GPUKernel::set_kernel_file_path(const std::string& kernel_file_path)
{
	m_kernel_file_path = kernel_file_path;
}

void GPUKernel::set_kernel_function_name(const std::string& kernel_function_name)
{
	m_kernel_function_name = kernel_function_name;
}

void GPUKernel::add_additional_macro_for_compilation(const std::string& name, int value)
{
	m_additional_compilation_macros[name] = value;
}

std::vector<std::string> GPUKernel::get_additional_compiler_macros() const
{
	std::vector<std::string> macros;

	for (auto macro_key_value : m_additional_compilation_macros)
		macros.push_back("-D " + macro_key_value.first + "=" + std::to_string(macro_key_value.second));

	return macros;
}

void GPUKernel::compile(std::shared_ptr<HIPRTOrochiCtx> hiprt_ctx, std::vector<hiprtFuncNameSet> func_name_sets, bool use_cache)
{
	if (m_option_macro_invalidated)
		parse_option_macros_used();

	std::string cache_key = g_gpu_kernel_compiler.get_additional_cache_key(*this);
	m_kernel_function = g_gpu_kernel_compiler.compile_kernel(*this, m_compiler_options, hiprt_ctx,
															 func_name_sets.data(), 
															 /* num geom */1,
															 /* num ray */ func_name_sets.size() == 0 ? 0 : 1,
															 use_cache, cache_key);
}

void GPUKernel::compile_silent(std::shared_ptr<HIPRTOrochiCtx> hiprt_ctx, std::vector<hiprtFuncNameSet> func_name_sets, bool use_cache)
{
	if (m_option_macro_invalidated)
		parse_option_macros_used();

	std::string cache_key = g_gpu_kernel_compiler.get_additional_cache_key(*this);
	m_kernel_function = g_gpu_kernel_compiler.compile_kernel(*this, m_compiler_options, hiprt_ctx, 
															 func_name_sets.data(),
															 /* num geom */1,
															 /* num rays */ func_name_sets.size() == 0 ? 0 : 1,
															 use_cache, cache_key, /* silent */ true);
}

int GPUKernel::get_kernel_attribute(oroFunction compiled_kernel, oroFunction_attribute attribute)
{
	int numRegs = 0;

	if (compiled_kernel == nullptr)
	{
		g_imgui_logger.add_line(ImGuiLoggerSeverity::IMGUI_LOGGER_ERROR, "Trying to get an attribute of a kernel that wasn't compiled yet.");

		return 0;
	}

	OROCHI_CHECK_ERROR(oroFuncGetAttribute(&numRegs, attribute, compiled_kernel));

	return numRegs;
}

int GPUKernel::get_kernel_attribute(oroFunction_attribute attribute)
{
	int numRegs = 0;

	if (m_kernel_function == nullptr)
	{
		g_imgui_logger.add_line(ImGuiLoggerSeverity::IMGUI_LOGGER_ERROR, "Trying to get an attribute of a kernel that wasn't compiled yet.");

		return 0;
	}

	OROCHI_CHECK_ERROR(oroFuncGetAttribute(&numRegs, ORO_FUNC_ATTRIBUTE_NUM_REGS, m_kernel_function));

	return numRegs;
}

GPUKernelCompilerOptions& GPUKernel::get_kernel_options()
{
	return m_compiler_options;
}

const GPUKernelCompilerOptions& GPUKernel::get_kernel_options() const
{
	return m_compiler_options;
}

void GPUKernel::synchronize_options_with(const GPUKernelCompilerOptions& other_options, const std::unordered_set<std::string>& options_excluded)
{
	for (auto macro_to_value : other_options.get_options_macro_map())
	{
		const std::string& macro_name = macro_to_value.first;
		int macro_value = *macro_to_value.second;

		if (options_excluded.find(macro_name) == options_excluded.end())
			// Option is not excluded
			m_compiler_options.set_pointer_to_macro(macro_name, other_options.get_pointer_to_macro_value(macro_name));
	}

	// Same thing with the custom macros
	for (auto macro_to_value : other_options.get_custom_macro_map())
	{
		const std::string& macro_name = macro_to_value.first;
		int macro_value = *macro_to_value.second;

		if (options_excluded.find(macro_name) == options_excluded.end())
			// Option is not excluded
			m_compiler_options.set_pointer_to_macro(macro_name, other_options.get_pointer_to_macro_value(macro_name));
	}
}

void GPUKernel::launch(int tile_size_x, int tile_size_y, int res_x, int res_y, void** launch_args, oroStream_t stream)
{
	hiprtInt2 nb_groups;
	nb_groups.x = std::ceil(static_cast<float>(res_x) / tile_size_x);
	nb_groups.y = std::ceil(static_cast<float>(res_y) / tile_size_y);

	OROCHI_CHECK_ERROR(oroModuleLaunchKernel(m_kernel_function, nb_groups.x, nb_groups.y, 1, tile_size_x, tile_size_y, 1, 0, stream, launch_args, 0));
}

void GPUKernel::launch_synchronous(int tile_size_x, int tile_size_y, int res_x, int res_y, void** launch_args, float* execution_time_out)
{
	OROCHI_CHECK_ERROR(oroEventRecord(m_execution_start_event, 0));

	launch(tile_size_x, tile_size_y, res_x, res_y, launch_args, 0);

	OROCHI_CHECK_ERROR(oroEventRecord(m_execution_stop_event, 0));
	OROCHI_CHECK_ERROR(oroEventSynchronize(m_execution_stop_event));
	OROCHI_CHECK_ERROR(oroEventElapsedTime(execution_time_out, m_execution_start_event, m_execution_stop_event));
}

void GPUKernel::parse_option_macros_used()
{
	m_used_option_macros = g_gpu_kernel_compiler.get_option_macros_used_by_kernel(*this);
	m_option_macro_invalidated = false;
}

bool GPUKernel::uses_macro(const std::string& name) const
{
	return m_used_option_macros.find(name) != m_used_option_macros.end();
}

float GPUKernel::get_last_execution_time()
{
	float out;
	oroEventElapsedTime(&out, m_execution_start_event, m_execution_stop_event);

	return out;
}

bool GPUKernel::has_been_compiled() const
{
	return m_kernel_function != nullptr;
}

bool GPUKernel::is_precompiled() const
{
	return m_is_precompiled_kernel;
}

void GPUKernel::set_precompiled(bool precompiled)
{
	m_is_precompiled_kernel = precompiled;
}

void GPUKernel::launch_asynchronous(int tile_size_x, int tile_size_y, int res_x, int res_y, void** launch_args, oroStream_t stream)
{
	OROCHI_CHECK_ERROR(oroEventRecord(m_execution_start_event, stream));

	launch(tile_size_x, tile_size_y, res_x, res_y, launch_args, stream);

	OROCHI_CHECK_ERROR(oroEventRecord(m_execution_stop_event, stream));

	// TODO: There's an issue here on HIP 5.7 + Windows where without the oroLaunchHostFunc below,
	// this oroEventRecord (or any event after a kernel launch) "blocks" the stream (only on a non-NULL stream)
	// and oroStreamQuery always (kind of) returns hipErrorDeviceNotReady
	oroLaunchHostFunc(stream, [](void*) {}, nullptr);
}
