#include "Driver/Sound.hpp"

#include <libdragon.h>

#include <string>

namespace SuperHaxagon {
	struct Sound::SoundData {
		SoundData(const std::string& path) {
			wav64_open(&sfx, (path + ".wav64").c_str());
		}

		~SoundData() {
			wav64_close(&sfx);
		}

		wav64_t sfx;
	};

	std::unique_ptr<Sound> createSound(const std::string& path) {
		auto data = std::make_unique<Sound::SoundData>(path);
		return std::make_unique<Sound>(std::move(data));
	}

	Sound::Sound(std::unique_ptr<SoundData> data) : _data(std::move(data)) {}
	
	Sound::~Sound() = default;

	void Sound::play() const {
		wav64_play(&(_data->sfx), 2);
	}
}
