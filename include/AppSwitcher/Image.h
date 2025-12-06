#pragma once

#include <cstdint>
#include <memory>
#include <string>

enum class ImageFormat : uint8_t {
	RGB,
	RGBA,
	BGRA,
};

struct Image {
	std::unique_ptr<uint8_t[]> buffer;
	uint32_t                   width;
	uint32_t                   height;
	ImageFormat                format;
};

Image read_image(std::string_view path, int size);
