#include <fcntl.h>
#include <gtest/gtest.h>

import std;
import llvm.Support;

import wm.AppInfoLoader;

using namespace wm;
namespace fs = llvm::sys::fs;
using namespace std::chrono_literals;

llvm::SmallString<256> make_temp_dir()
{
	llvm::SmallString<256> result;
	if (auto ec = fs::createUniqueDirectory("appinfo_test", result))
		throw std::runtime_error(ec.message());
	return result;
}

void write_desktop_file(llvm::StringRef dir, llvm::StringRef filename, llvm::StringRef content)
{
	if (auto ec = fs::create_directories(dir))
		throw std::runtime_error(ec.message());

	llvm::SmallString<256> path(dir);
	llvm::sys::path::append(path, filename);

	int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ASSERT_NE(fd, -1) << std::strerror(errno);

	auto written = write(fd, content.data(), content.size());
	close(fd);

	if (static_cast<size_t>(written) != content.size())
		throw std::runtime_error("short write to memfd");
}

class AppInfoLoaderTest : public testing::Test {
protected:
	llvm::SmallString<256> data_home;
	llvm::SmallString<256> data_dir1;
	llvm::SmallString<256> data_dir2;

	void SetUp() override
	{
		data_home = make_temp_dir();
		data_dir1 = make_temp_dir();
		data_dir2 = make_temp_dir();

		setenv("XDG_DATA_HOME", data_home.c_str(), 1);
		llvm::SmallString<256> dirs;
		std::format_to(std::back_inserter(dirs), "{}:{}", data_dir1.c_str(), data_dir2.c_str());
		setenv("XDG_DATA_DIRS", dirs.c_str(), 1);

		auto app_dir = [](llvm::StringRef base) {
			llvm::SmallString<256> path(base);
			llvm::sys::path::append(path, "applications");
			return path;
		};

		write_desktop_file(
		    app_dir(data_home), "foo.desktop", "[Desktop Entry]\nName=Foo\nIcon=foo\n"
		);

		write_desktop_file(
		    app_dir(data_dir1),
		    "bar.desktop",
		    "[Desktop Entry]\nName=Bar\nIcon=bar\nStartupWMClass=custom\n"
		);

		write_desktop_file(
		    app_dir(data_dir2), "bar.desktop", "[Desktop Entry]\nName=OtherBar\nIcon=otherbar\n"
		);
	}

	void TearDown() override
	{
		auto _ = fs::remove_directories(data_home);
		auto _ = fs::remove_directories(data_dir1);
		auto _ = fs::remove_directories(data_dir2);
	}
};

TEST_F(AppInfoLoaderTest, ScansAndResolvesAppInfo)
{
	AppInfoLoaderConfig config{.icon_size = 12, .icon_theme = ""};
	AppInfoLoader       loader(config);

	std::this_thread::sleep_for(1ms);
	if (!loader.is_available())
		FAIL() << "scan did not finish in time";

	auto foo = loader.get_app_info("foo");
	ASSERT_STREQ(foo.app_id, "foo");
	EXPECT_EQ(foo.name, "Foo");

	auto custom = loader.get_app_info("custom");
	ASSERT_STREQ(custom.app_id, "custom");
	EXPECT_EQ(custom.name, "Bar");

	auto bar = loader.get_app_info("bar");
	ASSERT_STREQ(bar.app_id, "bar");
	EXPECT_EQ(bar.name, "Bar");
}
