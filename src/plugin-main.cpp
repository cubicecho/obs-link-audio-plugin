// SPDX-License-Identifier: MIT

#include <obs-module.h>
#include <plugin-support.h>

#include "input/link-audio-input-source.hpp"
#include "session/link-session.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("PluginDescription");
}

bool obs_module_load(void)
{
	registerLinkAudioInputSource();
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	leaveSharedLinkSession();
	obs_log(LOG_INFO, "plugin unloaded");
}
