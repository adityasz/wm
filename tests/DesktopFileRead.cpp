#include <gtest/gtest.h>

#include <sys/mman.h>
#include <unistd.h>

import std;
import wm.AppInfoLoader.Xdg;

using std::size_t;
using namespace wm;

int make_memfd(std::string_view content)
{
	int fd = memfd_create("desktop-file-test", MFD_CLOEXEC);
	if (fd == -1) {
		throw std::runtime_error(std::strerror(errno));
	}
	auto written = write(fd, content.data(), content.size());
	if (static_cast<size_t>(written) != content.size()) {
		close(fd);
		throw std::runtime_error("short write to memfd");
	}
	if (lseek(fd, 0, SEEK_SET) == -1) {
		close(fd);
		throw std::runtime_error(std::strerror(errno));
	}
	return fd;
}

struct TestCase {
	std::string      test_name;
	std::string_view content;
	DesktopFileInfo  expected;
};

const TestCase cases[] = {
    {
        "Kitty",
        R"([Desktop Entry]
Version=1.0
Type=Application
Name=kitty
GenericName=Terminal emulator
Comment=Fast, feature-rich, GPU based terminal
TryExec=kitty
StartupNotify=true
Exec=kitty
Icon=kitty
)",
        {
            .name             = "kitty",
            .iconstring       = "kitty",
            .startup_wm_class = "",
        },
    },
    {"Spaces",
     R"([Desktop Entry]
StartupWMClass =no way
Name = terrible name
Icon= eye candy
)",
     {
         .name             = "terrible name",
         .iconstring       = "eye candy",
         .startup_wm_class = "no way",
     }},
};

class DesktopFileInfoTest : public testing::TestWithParam<TestCase> {};

TEST_P(DesktopFileInfoTest, ParsesExpectedFields)
{
	const auto &c = GetParam();

	int fd              = make_memfd(c.content);
	auto [buffer, size] = read_desktop_file(fd);
	close(fd);

	ASSERT_NE(buffer, nullptr);
	ASSERT_GT(size, 0);

	DesktopFileInfo info = get_desktop_file_info(buffer.get(), size);

	EXPECT_EQ(info, c.expected);
}

INSTANTIATE_TEST_SUITE_P(
    DesktopFiles,
    DesktopFileInfoTest,
    testing::ValuesIn(cases),
    [](const testing::TestParamInfo<TestCase> &info) { return info.param.test_name; }
);

TEST(ReadDesktopFileTest, ReturnsCorrectSizeAndContent)
{
	constexpr std::string_view content = "[Desktop Entry]\nName=test\n";
	int                        fd      = make_memfd(content);
	auto [buffer, size]                = read_desktop_file(fd);
	close(fd);

	ASSERT_NE(buffer, nullptr);
	ASSERT_EQ(static_cast<size_t>(size), content.size());
	EXPECT_EQ(std::string_view(buffer.get(), size), content);
}

TEST(ReadDesktopFileTest, HandlesEmptyFile)
{
	int fd              = make_memfd("");
	auto [buffer, size] = read_desktop_file(fd);
	close(fd);

	EXPECT_EQ(size, 0);
}
