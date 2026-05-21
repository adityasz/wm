export module wm.AppSwitcher.Image;

import std;

using std::uint8_t;
using std::uint32_t;

export namespace wm {
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
} // namespace wm
