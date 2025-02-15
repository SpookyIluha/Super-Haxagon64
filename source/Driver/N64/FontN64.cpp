#include "Driver/Font.hpp"

#include "Core/Structs.hpp"

#include <libdragon.h>

#include <sstream>
#include <algorithm>
#include <string>

#define FONT_TEXT_SMALL 1
#define FONT_TEXT_LARGE 2

namespace SuperHaxagon {
	struct Font::FontData {
		FontData(const std::string& path, int size) : size(size) {
			font = rdpq_font_load(path.c_str());
			rdpq_text_register_font(size > 26? FONT_TEXT_LARGE : FONT_TEXT_SMALL, font);

			// There's no text scaling available
			textScale = 1.0f;
		}

		~FontData() {
			rdpq_text_unregister_font(size > 26? FONT_TEXT_LARGE : FONT_TEXT_SMALL);
			rdpq_font_free(font);
		}
		
		int size;
		rdpq_font_t* font;
		bool    textScale = false;
	};


	std::unique_ptr<Font> createFont(const std::string& path, int size) {
		return std::make_unique<Font>(std::make_unique<Font::FontData>(path, size));
	}

	Font::Font(std::unique_ptr<Font::FontData> data) : _data(std::move(data)) {}

	Font::~Font() = default;

	void Font::setScale(const float scale) { 
		if(scale > 1.5f) _data->textScale = true;
		else _data->textScale = false;
	}

	float Font::getWidth(const std::string& str) const {
		std::string str_dynamic = str;
		float width;
		int len = str_dynamic.length();
		std::replace( str_dynamic.begin(), str_dynamic.end(), '$', 'S');
		std::replace( str_dynamic.begin(), str_dynamic.end(), '^', 'v');
		rdpq_paragraph_t * paragraph = rdpq_paragraph_build(NULL, _data->size > 26? FONT_TEXT_LARGE : FONT_TEXT_SMALL, str_dynamic.c_str(), &len);
		width = paragraph->bbox.x1 - paragraph->bbox.x0;
		rdpq_paragraph_free(paragraph);
		return width;
	}

	float Font::getHeight() const {
		return _data->size > 26? 32 : 16;
	}

	void Font::draw(const Color& color, const Point& position, const Alignment alignment, const std::string& str) const {
		std::string str_dynamic = str;
		std::replace( str_dynamic.begin(), str_dynamic.end(), '$', 'S');
		std::replace( str_dynamic.begin(), str_dynamic.end(), '^', 'v');
		rdpq_textparms_t parms = {0};
		float xpos = position.x;
		if (alignment == Alignment::LEFT) parms.align = ALIGN_LEFT;
		if (alignment == Alignment::CENTER) {
			parms.align = ALIGN_CENTER;
			xpos -= getWidth(str_dynamic.c_str()) / 2.0f;
		}
		if (alignment == Alignment::RIGHT) {
			parms.align = ALIGN_RIGHT;
			xpos -= getWidth(str_dynamic.c_str());
		}
		rdpq_fontstyle_t style;
		style.outline_color = RGBA32(0,0,0,255);
		style.color = RGBA32(color.r, color.g, color.b, color.a);
		rdpq_font_style(_data->font, 0, &style);
		rdpq_text_print(&parms, _data->size > 26? FONT_TEXT_LARGE : FONT_TEXT_SMALL, xpos, position.y + _data->size, str_dynamic.c_str());
	}
}
