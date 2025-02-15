#include "States/Load.hpp"

#include "Core/Game.hpp"
#include "Driver/Platform.hpp"
#include "Factories/LevelFactory.hpp"
#include "Factories/PatternFactory.hpp"
#include "States/Menu.hpp"
#include "States/Quit.hpp"

#include <memory>
#include <fstream>
#include <climits>

#include <libdragon.h>

class membuf : public std::basic_streambuf<char> {
	public:
	  membuf(const uint8_t *p, size_t l) {
		setg((char*)p, (char*)p, (char*)p + l);
	  }
	};

	class memstream : public std::istream {
		public:
		  memstream(const uint8_t *p, size_t l) :
			std::istream(&_buffer),
			_buffer(p, l) {
			rdbuf(&_buffer);
		  }
		
		private:
		  membuf _buffer;
		};


		uint8_t* readdata(uint8_t* ptr, uint8_t* dst, int size){
			int i = 0;
			for(; i < size; i++){
				dst[i] = ptr[i];
			}
			return ptr + i;
		}
		
		uint8_t* readdatastring(uint8_t* ptr, std::string& str) {
			int32_t len;
			ptr = readdata(ptr, (uint8_t*)reinterpret_cast<char*>(&len), sizeof(len));
			debugf("len %li\n", len);
			str = std::string(len, '\0');
			ptr = readdata(ptr, (uint8_t*)str.c_str(), len);
			return ptr;
		}

namespace SuperHaxagon {
	const char* Load::PROJECT_HEADER = "HAX1.1";
	const char* Load::PROJECT_FOOTER = "ENDHAX";
	const char* Load::SCORE_HEADER = "SCDB1.0";
	const char* Load::SCORE_FOOTER = "ENDSCDB";

	Load::Load(Game& game) : _game(game), _platform(game.getPlatform()) {}
	Load::~Load() = default;

	bool Load::loadLevels(std::istream& stream, Location location) const {
		std::vector<std::shared_ptr<PatternFactory>> patterns;

		// Used to make sure that external levels link correctly.
		const auto levelIndexOffset = _game.getLevels().size();

		if(!readCompare(stream, PROJECT_HEADER)) {
			_platform.message(Dbg::WARN, "file", "file header invalid!");
			return false;
		}

		const auto numPatterns = read32(stream, 1, 300, _platform, "number of patterns");
		patterns.reserve(numPatterns);
		for (auto i = 0; i < numPatterns; i++) {
			auto pattern = std::make_shared<PatternFactory>(stream, _platform);
			if (!pattern->isLoaded()) {
				_platform.message(Dbg::WARN, "file", "a pattern failed to load");
				return false;
			}

			patterns.emplace_back(std::move(pattern));
		}

		if (patterns.empty()) {
			_platform.message(Dbg::WARN, "file", "no patterns loaded");
			return false;
		}

		const auto numLevels = read32(stream, 1, 300, _platform, "number of levels");
		for (auto i = 0; i < numLevels; i++) {
			auto level = std::make_unique<LevelFactory>(stream, patterns, location, _platform, levelIndexOffset);
			if (!level->isLoaded()) {
				_platform.message(Dbg::WARN, "file", "a level failed to load");
				return false;
			}

			_game.addLevel(std::move(level));
		}

		if(!readCompare(stream, PROJECT_FOOTER)) {
			_platform.message(Dbg::WARN, "load", "file footer invalid");
			return false;
		}

		return true;
	}

	bool Load::loadScores(std::istream& stream, uint8_t* data) const {
		if (!stream) {
			_platform.message(Dbg::INFO, "scores", "no score database");
			return true;
		}

		if (!readCompare(stream, SCORE_HEADER)) {
			_platform.message(Dbg::WARN,"scores", "score header invalid, skipping scores");
			return true; // If there is no score database silently fail.
		}
		uint8_t* ptr = data + sizeof(SCORE_HEADER) + 3;
		//uint32_t dummy; ptr = readdata(ptr, (uint8_t*)&dummy, sizeof(dummy));
		//uint32_t dummy2; ptr = readdata(ptr, (uint8_t*)&dummy2, sizeof(dummy2));
		uint32_t numScores; ptr = readdata(ptr, (uint8_t*)&numScores, sizeof(numScores));
		debugf("numscores %li\n", numScores);
		for (uint32_t i = 0; i < numScores; i++) {
			std::string name; ptr = readdatastring(ptr, name);
			std::string difficulty; ptr = readdatastring(ptr, difficulty);
			std::string mode; ptr = readdatastring(ptr, mode);
			std::string creator; ptr = readdatastring(ptr, creator);
			int32_t score = 0; ptr = readdata(ptr, (uint8_t*)&score, sizeof(score));
			for (const auto& level : _game.getLevels()) {
				if (level->getName() == name && level->getDifficulty() == difficulty && level->getMode() == mode && level->getCreator() == creator) {
					level->setHighScore(score);
				}
			}
		}

		//if (!readCompare(stream, SCORE_FOOTER)) {
		//	_platform.message(Dbg::WARN,"scores", "file footer invalid, db broken");
		//	return false;
		//}

		return true;
	}

	void Load::enter() {
		std::vector<std::pair<Location, std::string>> levels;
		levels.emplace_back(Location::ROM, "/levels.haxagon");

		auto extra_levels = _platform.loadUserLevels();
		levels.insert(levels.end(), extra_levels.begin(), extra_levels.end());

		for (const auto& pair : levels) {
			const auto& path = pair.second;
			const auto location = pair.first;
			auto file = _platform.openFile(path, location);
			if (!file) continue;
			loadLevels(*file, location);
		}

		if (_game.getLevels().empty()) {
			_platform.message(Dbg::FATAL, "levels", "no levels loaded");
			return;
		}

		//std::ifstream scores(_platform.getPath("/scores.db", Location::USER), std::ios::in | std::ios::binary);
		debugf( "Reading '%s'\n", "/scores.db" );
		uint8_t* eepromfile = (uint8_t*)malloc(500);
		memset(eepromfile, 0, (size_t)500);
		
		const int result = eepfs_read("/scores.db", eepromfile, (size_t)500);
		if(result != EEPFS_ESUCCESS) debugf("eeprom file read unsuccessful\n");

		for(int i = 0; i < 500; i++){
			debugf("%x", eepromfile[i]);
		} debugf("\n");

		memstream stream(eepromfile, (size_t)500);
		if (!loadScores(stream, eepromfile)) return;
		free(eepromfile);

		_loaded = true;
	}

	std::unique_ptr<State> Load::update(float) {
		if (_loaded) return std::make_unique<Menu>(_game, *_game.getLevels()[0]);
		return std::make_unique<Quit>(_game);
	}
}
