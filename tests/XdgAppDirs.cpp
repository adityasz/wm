#include <gtest/gtest.h>

import std;
import wm.AppInfoLoader.Xdg;

using namespace wm;

void check(auto expected_dirs)
{
	auto app_dirs = get_xdg_app_dirs();
	ASSERT_EQ(app_dirs.dirs.size(), expected_dirs.size());
	for (auto [dir, expected] : std::views::zip(app_dirs.dirs, expected_dirs))
		ASSERT_STREQ(dir, expected);
}

TEST(XdgAppDirsTest, Defaults)
{
	setenv("HOME", "/home/foo", 1);
	unsetenv("XDG_DATA_HOME");
	unsetenv("XDG_DATA_DIRS");
	check(
	    std::array{
	        "/home/foo/.local/share/applications/",
	        "/usr/local/share/applications/",
	        "/usr/share/applications/"
	    }
	);
}

TEST(XdgAppDirsTest, EnvVars)
{
	setenv("XDG_DATA_HOME", "/f///", 1);
	setenv("XDG_DATA_DIRS", ":/a//:/b/c//::/d/e:", 1);
	check(
	    std::array{
	        "/f/applications/", "/a/applications/", "/b/c/applications/", "/d/e/applications/"
	    }
	);
}
