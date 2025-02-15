#include "Driver/Platform.hpp"

#include "Core/Structs.hpp"
#include "Core/Twist.hpp"
#include "Driver/Font.hpp"
#include "Driver/Music.hpp"
#include "Driver/Sound.hpp"
#include "Core/Game.hpp"

#include <libdragon.h>

#include <array>
#include <iostream>
#include <sstream>
#include <locale>
#include <codecvt>
#include <memory>
#include <deque>
#include <sys/stat.h>

bool fastMode = false;

namespace SuperHaxagon {
	std::unique_ptr<Font> createFont(const std::string& path, int size);
	std::unique_ptr<Music> createMusic(const std::string& path);
	std::unique_ptr<Sound> createSound(const std::string& path);

	extern void audioCallback(void*);

	struct Platform::PlatformData {

		bool transpState = false;
		bool debugConsole = false;
		float _last = 0;
	};

	Platform::Platform() : _plat(std::make_unique<PlatformData>()) {
		debug_init_isviewer();
		debug_init_usblog();
	
		debugf( "Starting...\n\n" );
		
		asset_init_compression(2);
		wav64_init_compression(3);
	
		dfs_init(DFS_DEFAULT_LOCATION);
	
		timer_init();
		rdpq_init();
		joypad_init();
		audio_init(28000, 8);
		mixer_init(4);

		int result = 0;
		const eeprom_type_t eeprom_type = eeprom_present();
		if(eeprom_type == EEPROM_4K){
			const eepfs_entry_t eeprom_4k_files[] = {
				{ "/scores.db", size_t(500)  },
			};
		
			debugf( "EEPROM Detected: 4 Kibit (64 blocks)\n" );
			debugf( "Initializing EEPROM Filesystem...\n" );
			result = eepfs_init(eeprom_4k_files, 1);
		}

		switch ( result )
		{
			case EEPFS_ESUCCESS:
			debugf( "Success!\n" );
				break;
			case EEPFS_EBADFS:
			debugf( "Failed with error: bad filesystem\n" );
			break;
			case EEPFS_ENOMEM: 
			debugf( "Failed with error: not enough memory\n" );
			break;
			default:
			debugf( "Failed in an unexpected manner\n" );
			break;
		}
		if ( !eepfs_verify_signature() )
		{
			/* If not, erase it and start from scratch */
			debugf( "Filesystem signature is invalid!\n" );
			debugf( "Wiping EEPROM...\n" );
			eepfs_wipe();
		}
		srand(getentropy32());
		register_VI_handler((void(*)())rand);

		joypad_inputs_t input = joypad_get_inputs(JOYPAD_PORT_1);
		if(input.btn.z) fastMode = true;
		display_init(((resolution_t){.width = 640, .height = 360, .interlaced = INTERLACE_HALF, .aspect_ratio = 16.0 / 9.0}), DEPTH_16_BPP, 5, GAMMA_NONE, fastMode? FILTERS_DISABLED : FILTERS_RESAMPLE_ANTIALIAS_DEDITHER);
		display_set_fps_limit(60);

	}

	Platform::~Platform() {
		timer_close();
		rdpq_close();
		joypad_close();
		audio_close();
		mixer_close();
	
		joypad_set_rumble_active(JOYPAD_PORT_1, false);
		unregister_VI_handler((void(*)())rand);
		display_close();
	}

	double _getNow() {
        double ticks = timer_ticks();
        return TICKS_TO_MS(ticks) / 1000.0f;
    }

	bool Platform::loop() {
		float t = _getNow();
		_delta = (t - _plat->_last);
		_plat->_last = t;
		if(_delta < (1.0 / display_get_refresh_rate())) wait_ticks((TICKS_FROM_MS((1 / display_get_refresh_rate()) * 1000) - t));
		return true;
	}

	float Platform::getDilation() const {
		return display_get_delta_time() / (1 / 50.0f);
	}

	std::string Platform::getPath(const std::string& partial, const Location location) const {
		switch (location) {
		case Location::ROM:
			return std::string("rom:") + partial;
		case Location::USER:
			return std::string("sd:/superhaxagon") + partial;
		}

		return "";
	}

	std::unique_ptr<std::istream> Platform::openFile(const std::string& partial, const Location location) const {
		return std::make_unique<std::ifstream>(getPath(partial, location), std::ios::in | std::ios::binary);
	}

	std::unique_ptr<Font> Platform::loadFont(int size) const {
		std::stringstream s;
		s << "/fonts/bump-it-up-" << size << ".font64";
		return createFont(getPath(s.str(), Location::ROM), size);
	}

	std::unique_ptr<Sound> Platform::loadSound(const std::string& base) const {
		return createSound(getPath(base, Location::ROM));
	}

	std::unique_ptr<Music> Platform::loadMusic(const std::string& base, Location location) const {
		return createMusic(getPath(base, location));
	}

	std::string Platform::getButtonName(const Buttons& button) {
		if (button.back) return "B";
		if (button.select) return "A / START";
		if (button.left) return "LEFT";
		if (button.right) return "RIGHT";
		if (button.quit) return "QUIT";
		return "?";
	}

	Buttons Platform::getPressed() const {
		joypad_poll();
		joypad_buttons_t input = joypad_get_buttons_pressed(JOYPAD_PORT_1);
		joypad_inputs_t stick = joypad_get_inputs(JOYPAD_PORT_1);
		Buttons buttons{};
		buttons.select = input.a || input.start;
		buttons.back = input.b || input.start;
		buttons.quit = false;
		buttons.left = stick.btn.d_left || stick.btn.c_left || stick.stick_x < -5;
		buttons.right = stick.btn.d_right || stick.btn.c_right || stick.stick_x > 5;
		if(input.a || input.b || input.start) addRumble(0.3f);
		return buttons;
	}

	Point Platform::getScreenDim() const {
		return {(float)display_get_width(),(float)display_get_height()};
	}

	void Platform::screenBegin() const {
		rdpq_attach(display_try_get(), NULL);

		rdpq_set_mode_standard();
		rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
		rdpq_mode_dithering(DITHER_SQUARE_INVSQUARE);
		if(!fastMode) rdpq_mode_antialias(AA_REDUCED);
		
		mixer_try_play();
		_plat->transpState = false;
	}

	void Platform::screenSwap() {

	}

	void Platform::screenFinalize() const {
		joypad_inputs_t stick = joypad_get_inputs(JOYPAD_PORT_1);
		if(stick.btn.l || stick.btn.r) rdpq_text_printf(NULL, 1, 50,100, "FPS: %.2f", display_get_fps());
		rdpq_detach_show();
	}

	void Platform::drawPoly(const Color& color, const std::vector<Point>& points) const {
		// Handle transparency state
		if(!fastMode){
			if(color.a < 255 && !_plat->transpState){
				rdpq_set_mode_standard();
				rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
				rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
				rdpq_mode_dithering(DITHER_SQUARE_INVSQUARE);
				rdpq_mode_antialias(AA_REDUCED);
				_plat->transpState = true;
			} else if (color.a == 255 && _plat->transpState){
				rdpq_set_mode_standard();
				rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
				rdpq_mode_dithering(DITHER_SQUARE_INVSQUARE);
				rdpq_mode_antialias(AA_REDUCED);
				_plat->transpState = false;
			}
		}

		rdpq_set_prim_color(RGBA32(color.r, color.g, color.b, color.a));
		for (size_t i = 1; i < points.size() - 1; i++) {

			float v1[] = { points[0].x, points[0].y };
			float v2[] = { points[i].x, points[i].y };
			float v3[] = { points[i + 1].x, points[i + 1].y };

			rdpq_triangle(&TRIFMT_FILL, v1, v2, v3);
		}
	}

	std::unique_ptr<Twist> Platform::getTwister() {
		// Kind of a shitty way to do this, but it's the best I got.
		const auto a = new std::seed_seq{timer_ticks()};
		return std::make_unique<Twist>(
			std::unique_ptr<std::seed_seq>(a)
		);
	}

	void Platform::shutdown() {
		assertf(0, "Platform::shutdown");
	}

	void Platform::message(const Dbg dbg, const std::string& where, const std::string& message) const {
		std::string format;
		if (dbg == Dbg::INFO) {
			format = "[N64:INFO] ";
		} else if (dbg == Dbg::WARN) {
			format = "[N64:WARN] ";
		} else if (dbg == Dbg::FATAL) {
			format = "[N64:FATAL] ";
		}

		format += where + ": " + message + "\n";

		debugf(format.c_str());
	}

	Supports Platform::supports() {
		return fastMode? Supports::NOTHING : Supports::SHADOWS;
	}
}
