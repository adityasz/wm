#pragma once

#include "Globals.h"

void focus_and_raise_window(
    const PHLWINDOW              &window,
    const SP<CWLSurfaceResource> &pSurface             = nullptr,
    bool                          preserveFocusHistory = false
);

template <typename T>
void add_config(std::string_view key, T value)
{
	HyprlandAPI::addConfigValue(PHANDLE, std::format("plugin:wm:{}", key), value);
}

template <typename T>
const T *const *get_config(std::string_view key)
{
	return reinterpret_cast<const T *const *>(
	    HyprlandAPI::getConfigValue(PHANDLE, std::format("plugin:wm:{}", key))->getDataStaticPtr()
	);
}
