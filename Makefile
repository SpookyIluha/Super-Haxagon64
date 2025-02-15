# Directories

BUILD_DIR=build

include $(N64_INST)/include/n64.mk

N64_CXXFLAGS += -Isource/ -O3 -ffunction-sections -fdata-sections

# File aggregators
SRCS		:= source/main.cpp

# Game sources
include ./openhexagonsrcsMk.txt
include ./openhexagonsrcsN64Mk.txt

assets_png = $(wildcard assets/textures/*.png)
assets_music = $(wildcard assets/bgm/*.wav)
assets_wav = $(wildcard assets/sound/*.wav)
assets_haxagon = $(wildcard assets/*.haxagon)

assets_txt = $(wildcard assets/bgm/*.txt)
assets_fonts = $(wildcard assets/fonts/*.ttf)

assets_conv = $(addprefix filesystem/textures/,$(notdir $(assets_png:%.png=%.sprite))) \
			  $(addprefix filesystem/bgm/,$(notdir $(assets_music:%.wav=%.wav64))) \
			  $(addprefix filesystem/sound/,$(notdir $(assets_wav:%.wav=%.wav64))) \
			  $(addprefix filesystem/,$(notdir $(assets_haxagon:%.haxagon=%.haxagon))) \
			  $(addprefix filesystem/bgm/,$(notdir $(assets_txt:%.txt=%.txt))) \
			  $(addprefix filesystem/fonts/,$(notdir $(assets_fonts:%.ttf=%.font64))) 

all: SuperHaxagon64.z64

filesystem/textures/%.sprite: assets/textures/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -d -o filesystem/textures "$<"

filesystem/%.haxagon: assets/%.haxagon
	@mkdir -p $(dir $@)
	@echo "    [HAXAGON] $@"
	cp "$<" $@

filesystem/bgm/%.txt: assets/bgm/%.txt
	@mkdir -p $(dir $@)
	@echo "    [TXT] $@"
	cp "$<" $@

filesystem/sound/%.wav64: assets/sound/%.wav
	@mkdir -p $(dir $@)
	@echo "    [AUDIO] $@"
	@$(N64_AUDIOCONV) --wav-compress 1 -o filesystem/sound $<

filesystem/bgm/%.wav64: assets/bgm/%.wav
	@mkdir -p $(dir $@)
	@echo "    [MUSIC] $@"
	@$(N64_AUDIOCONV) --wav-compress 1 -o filesystem/bgm $<

filesystem/fonts/%.font64: assets/fonts/%.ttf
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	$(N64_MKFONT) $(MKFONT_FLAGS) -o filesystem/fonts/ "$<"

filesystem/fonts/bump-it-up-16.font64: MKFONT_FLAGS+=--size 18 --outline 2
filesystem/fonts/bump-it-up-32.font64: MKFONT_FLAGS+=--size 36 --outline 2


$(BUILD_DIR)/SuperHaxagon64.dfs: $(assets_conv)
$(BUILD_DIR)/SuperHaxagon64.elf: $(SRCS:%.cpp=$(BUILD_DIR)/%.o)

SuperHaxagon64.z64: N64_ROM_TITLE="Starship Madness 64"
SuperHaxagon64.z64: $(BUILD_DIR)/SuperHaxagon64.dfs
SuperHaxagon64.z64: N64_ROM_SAVETYPE = eeprom4k

clean:
	rm -rf $(BUILD_DIR) *.z64
	rm -rf filesystem

build_lib:
	rm -rf $(BUILD_DIR) *.z64
	make -C $(T3D_INST)
	make all

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean