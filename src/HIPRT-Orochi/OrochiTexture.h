/*
 * Copyright 2024 Tom Clabault. GNU GPL3 license.
 * GNU GPL3 license copy: https://www.gnu.org/licenses/gpl-3.0.txt
 */

#ifndef OROCHI_TEXTURE_H
#define OROCHI_TEXTURE_H

#include "HIPRT-Orochi/OrochiBuffer.h"
#include "Image/Image.h"

class OrochiTexture
{
public:
	OrochiTexture() {}
	OrochiTexture(const Image8Bit& image, HIPfilter_mode filtering_mode = ORO_TR_FILTER_MODE_POINT);
	OrochiTexture(const Image32Bit& image, HIPfilter_mode filtering_mode = ORO_TR_FILTER_MODE_POINT);
	OrochiTexture(const OrochiTexture& other) = delete;
	OrochiTexture(OrochiTexture&& other) noexcept;
	~OrochiTexture();

	void operator=(const OrochiTexture& other) = delete;
	void operator=(OrochiTexture&& other) noexcept;

	void init_from_image(const Image8Bit& image, HIPfilter_mode filtering_mode = ORO_TR_FILTER_MODE_POINT);
	void init_from_image(const Image32Bit& image, HIPfilter_mode filtering_mode = ORO_TR_FILTER_MODE_POINT);

	oroTextureObject_t get_device_texture();

	unsigned int width = 0, height = 0;

private:
	oroArray_t m_texture_array = nullptr;

	oroTextureObject_t m_texture = nullptr;
};

#endif
