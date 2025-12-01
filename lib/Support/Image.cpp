module;

#include <librsvg/rsvg.h>
#include <spng.h>
#include <turbojpeg.h>

module wm.Support.Image;

import std;

namespace wm {
static std::vector<uint8_t> read_file(std::string_view path)
{
	try {
		std::ifstream file(std::string(path), std::ios::binary | std::ios::ate);
		if (!file)
			return {};
		auto size = file.tellg();
		file.seekg(0);
		std::vector<uint8_t> buffer(size);
		file.read(reinterpret_cast<char *>(buffer.data()), size);
		return buffer;
	} catch (...) {
		return {};
	}
}

static Image load_png(std::span<uint8_t> data)
{
	if (data.empty())
		return {};

	spng_ctx *ctx = spng_ctx_new(0);
	if (!ctx)
		return {};

	spng_set_png_buffer(ctx, data.data(), data.size());

	spng_ihdr ihdr{};
	if (spng_get_ihdr(ctx, &ihdr)) {
		spng_ctx_free(ctx);
		return {};
	}

	int fmt = ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR_ALPHA
	                  || ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE_ALPHA
	              ? SPNG_FMT_RGBA8
	              : SPNG_FMT_RGB8;

	size_t out_size;
	if (spng_decoded_image_size(ctx, fmt, &out_size)) {
		spng_ctx_free(ctx);
		return {};
	}

	auto buffer = std::make_unique<uint8_t[]>(out_size);
	if (spng_decode_image(ctx, buffer.get(), out_size, fmt, 0)) {
		spng_ctx_free(ctx);
		return {};
	}

	spng_ctx_free(ctx);

	return {
	    std::move(buffer),
	    static_cast<int32_t>(ihdr.width),
	    static_cast<int32_t>(ihdr.height),
	    fmt == SPNG_FMT_RGBA8 ? ImageFormat::RGBA : ImageFormat::RGB
	};
}

// TODO: untested LLM written code
static Image load_jpg(std::span<uint8_t> data)
{
	if (data.empty())
		return {};

	tjhandle handle = tjInitDecompress();
	if (!handle)
		return {};

	int width, height, subsamp, colorspace;
	if (tjDecompressHeader3(
	        handle, data.data(), data.size(), &width, &height, &subsamp, &colorspace
	    )) {
		tjDestroy(handle);
		return {};
	}

	auto buffer = std::make_unique<uint8_t[]>(width * height * 3);
	if (tjDecompress2(
	        handle, data.data(), data.size(), buffer.get(), width, 0, height, TJPF_RGB, 0
	    )) {
		tjDestroy(handle);
		return {};
	}

	tjDestroy(handle);

	return {
	    std::move(buffer),
	    width,
	    height,
	    ImageFormat::RGB
	};
}

static Image load_svg(const char *path, int target_size)
{
	GError *gerror = nullptr;
	GFile  *gfile  = g_file_new_for_path(path);
	if (!gfile)
		return {};
	RsvgHandle *handle =
	    rsvg_handle_new_from_gfile_sync(gfile, RSVG_HANDLE_FLAGS_NONE, nullptr, &gerror);
	if (!handle) {
		if (gerror)
			g_error_free(gerror);
		return {};
	}

	double   svg_width, svg_height;
	gboolean has_size = rsvg_handle_get_intrinsic_size_in_pixels(handle, &svg_width, &svg_height);

	int width, height;
	if (has_size) {
		double scale = static_cast<double>(target_size) / std::max(svg_width, svg_height);
		width        = static_cast<int>(std::ceil(svg_width * scale));
		height       = static_cast<int>(std::ceil(svg_height * scale));
	} else {
		width = height = target_size;
	}

	auto             buffer  = std::make_unique<uint8_t[]>(width * height * 4);
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
	    buffer.get(), CAIRO_FORMAT_ARGB32, width, height, width * 4
	);
	cairo_t *cr = cairo_create(surface);

	RsvgRectangle viewport = {0.0, 0.0, static_cast<double>(width), static_cast<double>(height)};
	if (!rsvg_handle_render_document(handle, cr, &viewport, &gerror)) {
		cairo_destroy(cr);
		cairo_surface_destroy(surface);
		g_object_unref(handle);
		if (gerror)
			g_error_free(gerror);
		return {};
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	g_object_unref(handle);

	return {
	    std::move(buffer),
	    width,
	    height,
	    ImageFormat::BGRA
	};
}

Image read_image(std::string_view path, int size)
{
	if (path.ends_with(".svg"))
		return load_svg(std::string(path).c_str(), size);
	auto data = read_file(path);
	if (path.ends_with(".png"))
		return load_png(data);
	if (path.ends_with(".jpg") || path.ends_with(".jpeg"))
		return load_jpg(data);
	return {};
}
} // namespace wm
