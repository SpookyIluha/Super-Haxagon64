#include "Driver/Music.hpp"

#include <libdragon.h>

#include <string>

namespace SuperHaxagon {
	struct Music::MusicData {
		MusicData(const std::string& path) {
			wav64_open(&music, (path + ".wav64").c_str());
		}

		~MusicData() {
			wav64_close(&music);
		}

		wav64_t music;

		// Timing controls
		bool  loop = false;
		float start = 0;
		float diff = 0;
	};

    double getNow() {
        double ticks = timer_ticks();
        return TICKS_TO_MS(ticks) / 1000.0f;
    }

	std::unique_ptr<Music> createMusic(const std::string& path) {
		auto data = std::make_unique<Music::MusicData>(path);
		return std::make_unique<Music>(std::move(data));
	}

	Music::Music(std::unique_ptr<Music::MusicData> data) : _data(std::move(data)) {}

	Music::~Music() {
		mixer_ch_stop(0);
	}

	void Music::update() const {
		mixer_try_play();
	}

	void Music::setLoop(const bool loop) const {
		_data->loop = loop;
		wav64_set_loop(&(_data->music), loop);
	}

	void Music::play() const {
		_data->start = getNow();
		wav64_play(&(_data->music), 0);

		if (_data->diff > 0) _data->start += getNow() - _data->diff;
		_data->diff = 0;
		mixer_ch_set_vol(0, 0.5, 0.5);
	}

	void Music::pause() const {
		mixer_ch_stop(0);
	}

	bool Music::isDone() const {
		return !mixer_ch_playing(0);
	}

	float Music::getTime() const {
		return mixer_ch_get_pos(0) / _data->music.wave.frequency;
	}
}
