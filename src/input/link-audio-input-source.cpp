// SPDX-License-Identifier: MIT

#include "input/link-audio-input-source.hpp"

#include <string>

#include <obs-module.h>

#include "input/link-audio-input.hpp"
#include "session/link-session.hpp"

namespace {

const char *const SOURCE_ID = "link_audio_input";
const char *const SETTING_CHANNEL = "channel";
const char *const NO_CHANNEL = "";

const char *getName(void *)
{
	return obs_module_text("LinkAudioInput");
}

void update(void *data, obs_data_t *settings)
{
	auto *input = static_cast<LinkAudioInput *>(data);
	input->selectChannel(obs_data_get_string(settings, SETTING_CHANNEL));
}

void *create(obs_data_t *settings, obs_source_t *source)
{
	auto *input = new LinkAudioInput(source, sharedLinkSession());
	update(input, settings);
	return input;
}

void destroy(void *data)
{
	delete static_cast<LinkAudioInput *>(data);
}

void getDefaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, SETTING_CHANNEL, NO_CHANNEL);
}

obs_properties_t *getProperties(void *data)
{
	auto *input = static_cast<LinkAudioInput *>(data);
	obs_properties_t *properties = obs_properties_create();
	obs_property_t *channelList = obs_properties_add_list(properties, SETTING_CHANNEL, obs_module_text("Channel"),
							      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(channelList, obs_module_text("NoChannel"), NO_CHANNEL);
	// OBS asks for a source type's properties without an instance when it only needs their layout.
	if (!input) {
		return properties;
	}

	for (const ChannelChoice &choice : input->channelChoices()) {
		const std::string offlineName = choice.label + " " + obs_module_text("Offline");
		const std::string &shownName = choice.isOnline ? choice.label : offlineName;
		obs_property_list_add_string(channelList, shownName.c_str(), choice.label.c_str());
	}

	return properties;
}

} // namespace

void registerLinkAudioInputSource()
{
	obs_source_info info = {};
	info.id = SOURCE_ID;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.icon_type = OBS_ICON_TYPE_AUDIO_INPUT;
	info.get_name = getName;
	info.create = create;
	info.destroy = destroy;
	info.update = update;
	info.get_defaults = getDefaults;
	info.get_properties = getProperties;

	obs_register_source(&info);
}
