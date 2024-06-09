/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef OPEN_IMAGE_DENOISER
#define OPEN_IMAGE_DENOISER

#include "HostDeviceCommon/Color.h"
#include "OpenGL/OpenGLInteropBuffer.h"

#include <OpenImageDenoise/oidn.hpp>
#include <vector>

class OpenImageDenoiser
{
public:
	OpenImageDenoiser();

	void set_use_albedo(bool use_albedo);
	void set_use_normals(bool use_normal);

	void resize(int new_width, int new_height);

	void initialize();
	/**
	 * Function that finalizes the creation of the internal denoising
	 * filters etc... once everything is setup (set_use_albedo / set_use_normals
	 * have been called if necessary, subsequent buffers have been provided, ...)
	*/
	void finalize();

	void denoise(std::shared_ptr<OpenGLInteropBuffer<ColorRGB>> data_to_denoise);

	void copy_denoised_data_to_buffer(std::shared_ptr<OpenGLInteropBuffer<ColorRGB>> buffer);


private:
	void create_device();
	bool check_device();

	bool m_use_albedo;
	bool m_use_normals;

	int m_width, m_height;

	oidn::DeviceRef m_device;

	oidn::FilterRef m_beauty_filter;
	oidn::FilterRef m_albedo_filter;
	oidn::FilterRef m_normals_filter;

	oidn::BufferRef m_input_color_buffer_oidn;
	oidn::BufferRef m_denoised_buffer;
};

#endif
