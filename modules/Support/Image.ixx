export module wm.Support.Image;

import std;

using std::uint8_t;
using std::int32_t;

export namespace wm {
enum class ImageFormat : uint8_t {
	RGB,
	RGBA,
	BGRA,
};

struct Image {
	std::unique_ptr<uint8_t[]> buffer;
	int32_t                    width;
	int32_t                    height;
	ImageFormat                format;
};

Image read_image(std::string_view path, int size);
} // namespace wm
