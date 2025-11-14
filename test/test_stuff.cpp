#include <gtest/gtest.h>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <opencv4/opencv2/imgproc.hpp>

#include "Image.h"

static bool save_image(const Image &img, const std::string &filename)
{
	if (!img.buffer)
		return false;

	int cvType = 0;

	switch (img.format) {
	case ImageFormat::RGB:  cvType = CV_8UC3; break;
	case ImageFormat::RGBA:
	case ImageFormat::BGRA: cvType = CV_8UC4; break;
	}

	cv::Mat mat(
	    static_cast<int>(img.height), static_cast<int>(img.width), cvType, img.buffer.get()
	);
	cv::Mat to_save;

	switch (img.format) {
	case ImageFormat::RGB:  cv::cvtColor(mat, to_save, cv::COLOR_RGB2BGR); break;
	case ImageFormat::RGBA: cv::cvtColor(mat, to_save, cv::COLOR_RGBA2BGRA); break;
	case ImageFormat::BGRA: to_save = mat;
	}

	return cv::imwrite(filename, to_save);
}

TEST(TestSvgRead, kitty)
{
	auto image = read_image("/usr/share/icons/hicolor/scalable/apps/kitty.svg", 137);
	EXPECT_TRUE(
	    save_image(image, "/tmp/kitty.png")
	); // then manually see to confirm; too bad, but better than nothing
}

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
