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
		float pausepos = -1;
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

		if(_data->pausepos > 0)
		mixer_try_play();
	}

	void Music::setLoop(const bool loop) const {
		_data->loop = loop;
		wav64_set_loop(&(_data->music), loop);
	}

	void Music::play() const {
		if(_data->pausepos > 0){
			_data->pausepos = -1;
		} else {
			_data->start = getNow();
			wav64_play(&(_data->music), 0);
			return;
		}

		if (_data->diff > 0) _data->start += getNow() - _data->diff;
		_data->diff = 0;
		mixer_ch_set_vol(0, 0.75, 0.75);
	}

	void Music::pause() const {
		_data->pausepos = 1;
		//mixer_ch_set_vol(0, 0, 0);
	}

	bool Music::isDone() const {
		float len = (float)_data->music.wave.len / _data->music.wave.frequency;
		return getTime() > len;
	}

	float Music::getTime() const {
		return _data->diff > 0 ? _data->diff - _data->start : getNow() - _data->start;
	}
}
