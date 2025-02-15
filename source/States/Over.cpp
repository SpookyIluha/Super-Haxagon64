#include "States/Over.hpp"

#include "Core/Game.hpp"
#include "Driver/Font.hpp"
#include "Driver/Platform.hpp"
#include "Factories/LevelFactory.hpp"
#include "Objects/Level.hpp"
#include "States/Load.hpp"
#include "States/Menu.hpp"
#include "States/Play.hpp"
#include "States/Quit.hpp"

#include <cstring>
#include <ostream>
#include <fstream>

#include <libdragon.h>

class membuf : public std::basic_streambuf<char> {
	public:
	  membuf(const uint8_t *p, size_t l) {
		setg((char*)p, (char*)p, (char*)p + l);
	  }
	};

	class omemstream : public std::ostream {
		public:
		  omemstream(const uint8_t *p, size_t l) :
			std::ostream(&_buffer),
			_buffer(p, l) {
			rdbuf(&_buffer);
		  }
		
		private:
		  membuf _buffer;
		};

uint8_t* writedata(uint8_t* ptr, uint8_t* src, int size){
	int i = 0;
	for(; i < size; i++){
		ptr[i] = src[i];
	}
	return ptr + i;
}

uint8_t* writedatastring(uint8_t* ptr, const std::string& str) {
	auto len = static_cast<uint32_t>(str.length());
	ptr = writedata(ptr, (uint8_t*)reinterpret_cast<char*>(&len), sizeof(len));
	ptr = writedata(ptr, (uint8_t*)str.c_str(), str.length());
	return ptr;
}

namespace SuperHaxagon {

	Over::Over(Game& game, std::unique_ptr<Level> level, LevelFactory& selected, const float score, std::string text) :
		_game(game),
		_platform(game.getPlatform()),
		_selected(selected),
		_level(std::move(level)),
		_text(std::move(text)),
		_score(score) {
		_high = _selected.setHighScore(static_cast<int>(score));
	}

	Over::~Over() = default;

	void Over::enter() {
		_game.playEffect(SoundEffect::OVER);

		printf( "Writing '%s'\n", "/scores.db" );
		uint8_t* eepromfile = (uint8_t*)malloc(500);
		memset(eepromfile, 0, (size_t)500);

		uint8_t* pos = eepromfile;

		pos = writedata(pos, (uint8_t*)Load::SCORE_HEADER, strlen(Load::SCORE_HEADER));
		auto levels = static_cast<uint32_t>(_game.getLevels().size());
		pos = writedata(pos, (uint8_t*)reinterpret_cast<char*>(&levels), sizeof(levels));

		for (const auto& lev : _game.getLevels()) {
			pos = writedatastring(pos, lev->getName());
			pos = writedatastring(pos, lev->getDifficulty());
			pos = writedatastring(pos, lev->getMode());
			pos = writedatastring(pos, lev->getCreator());
			uint32_t highSc = lev->getHighScore();
			pos = writedata(pos, (uint8_t*)reinterpret_cast<char*>(&highSc), sizeof(highSc));
		}

		pos = writedata(pos, (uint8_t*)Load::SCORE_FOOTER, strlen(Load::SCORE_FOOTER));

		debugf( "Writing '%s'\n", "/scores.db" );
		for(int i = 0; i < 500; i++){
			debugf("%x", eepromfile[i]);
		} debugf("\n");
		
		if(eepfs_write("/scores.db", eepromfile, (size_t)500) == EEPFS_ESUCCESS)
			debugf("writing successful\n");
		else debugf("writing unsuccessful\n");

		free(eepromfile);
	}

	std::unique_ptr<State> Over::update(const float dilation) {
		_frames += dilation;
		_level->rotate(GAME_OVER_ROT_SPEED, dilation);
		_level->clamp();

		const auto press = _platform.getPressed();
		if(press.quit) return std::make_unique<Quit>(_game);

		if(_frames <= FRAMES_PER_GAME_OVER) {
			_offset *= GAME_OVER_ACCELERATION_RATE * dilation + 1.0f;
		}

		if(_frames >= FRAMES_PER_GAME_OVER) {
			_level->clearPatterns();
			if (press.select) {
				// If the level we are playing is not the same as the index, we need to load
				// the original music
				if (_selected.getMusic() != _level->getLevelFactory().getMusic()) {
					_game.playMusic(_selected.getMusic(), _selected.getLocation(), true);
				}

				// Go back to the original level
				return std::make_unique<Play>(_game, _selected, _selected, 0.0f);
			}

			if (press.back) {
				return std::make_unique<Menu>(_game, _selected);
			}
		}

		return nullptr;
	}

	void Over::drawTop(const float scale) {
		_level->draw(_game, scale, _offset);
	}

	void Over::drawBot(const float scale) {
		auto& large = _game.getFontLarge();
		auto& small = _game.getFontSmall();
		large.setScale(scale);
		small.setScale(scale);

		const auto padText = 3 * scale;
		const auto margin = 20 * scale;
		const auto width = _platform.getScreenDim().x;
		const auto height = _platform.getScreenDim().y;
		const auto heightLarge = large.getHeight();
		const auto heightSmall = small.getHeight();

		const Point posGameOver = {width / 2, margin};
		const Point posTime = {width / 2, posGameOver.y + heightLarge + padText};
		const Point posBest = {width / 2, posTime.y + heightSmall + padText};
		const Point posB = {width / 2, height - margin - heightSmall};
		const Point posA = {width / 2, posB.y - heightSmall - padText};

		const auto textScore = std::string("TIME: ") + getTime(_score);
		large.draw(COLOR_WHITE, posGameOver, Alignment::CENTER,  _text);
		small.draw(COLOR_WHITE, posTime, Alignment::CENTER, textScore);

		if(_high) {
			const auto percent = getPulse(_frames, PULSE_TIME, 0);
			const auto pulse = interpolateColor(PULSE_LOW, PULSE_HIGH, percent);
			small.draw(pulse, posBest, Alignment::CENTER, "NEW RECORD!");
		} else {
			const auto score = _selected.getHighScore();
			const auto textBest = std::string("BEST: ") + getTime(static_cast<float>(score));
			small.draw(COLOR_WHITE, posBest, Alignment::CENTER, textBest);
		}

		if(_frames >= FRAMES_PER_GAME_OVER) {
			Buttons a{};
			a.select = true;
			small.draw(COLOR_WHITE, posA, Alignment::CENTER, "PRESS (" + _platform.getButtonName(a) + ") TO PLAY");

			Buttons b{};
			b.back = true;
			small.draw(COLOR_WHITE, posB, Alignment::CENTER, "PRESS (" + _platform.getButtonName(b) + ") TO QUIT");
		}
	}
}
