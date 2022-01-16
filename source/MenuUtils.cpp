#include <malloc.h>
#include <cstdint>
#include <cstring>
#include <string>

#include "MenuUtils.h"
#include "DrawUtils.h"

#include <coreinit/screen.h>
#include <coreinit/debug.h>
#include <vpad/input.h>
#include <gx2/state.h>

#include "icon_png.h"

const char *menu_options[] = {
        "Wii U Menu",
        "Homebrew Launcher",
        "vWii System Menu",
        "vWii Homebrew Channel",
};

const char *autoboot_config_strings[] = {
        "wiiu_menu",
        "homebrew_launcher",
        "vwii_system_menu",
        "vwii_homebrew_channel",
};


int32_t readAutobootOption(std::string &configPath) {
    FILE *f = fopen(configPath.c_str(), "r");
    if (f) {
        char buf[128]{};
        fgets(buf, sizeof(buf), f);
        fclose(f);

        for (uint32_t i = 0; i < sizeof(autoboot_config_strings) / sizeof(char *); i++) {
            if (strncmp(autoboot_config_strings[i], buf, strlen(autoboot_config_strings[i])) == 0) {
                return i;
            }
        }
    }
    return -1;
}

void writeAutobootOption(std::string &configPath, int32_t autobootOption) {
    FILE *f = fopen(configPath.c_str(), "w");
    if (f) {
        if (autobootOption >= 0) {
            fputs(autoboot_config_strings[autobootOption], f);
        } else {
            fputs("none", f);
        }

        fclose(f);
    }
}

int32_t handleMenuScreen(std::string &configPath, int32_t autobootOptionInput) {
    auto screenBuffer = DrawUtils::InitOSScreen();
    if (!screenBuffer) {
        OSFatal("Failed to alloc memory for screen");
    }

    uint32_t tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    DrawUtils::initBuffers(screenBuffer, tvBufferSize, screenBuffer + tvBufferSize, drcBufferSize);
    DrawUtils::initFont();

    uint32_t selected = autobootOptionInput > 0 ? autobootOptionInput : 0;
    int autoboot = autobootOptionInput;
    bool redraw = true;
    while (true) {
        VPADStatus vpad{};
        VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);

        if (vpad.trigger & VPAD_BUTTON_UP) {
            if (selected > 0) {
                selected--;
                redraw = true;
            }
        } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
            if (selected < sizeof(menu_options) / sizeof(char *) - 1) {
                selected++;
                redraw = true;
            }
        } else if (vpad.trigger & VPAD_BUTTON_A) {
            break;
        } else if (vpad.trigger & VPAD_BUTTON_X) {
            autoboot = -1;
            redraw = true;
        } else if (vpad.trigger & VPAD_BUTTON_Y) {
            autoboot = selected;
            redraw = true;
        }

        if (redraw) {
            DrawUtils::beginDraw();
            DrawUtils::clear(COLOR_BACKGROUND);

            // draw buttons
            uint32_t index = 8 + 24 + 8 + 4;
            for (uint32_t i = 0; i < sizeof(menu_options) / sizeof(char *); i++) {
                if (i == selected) {
                    DrawUtils::drawRect(16, index, SCREEN_WIDTH - 16 * 2, 44, 4, COLOR_BORDER_HIGHLIGHTED);
                } else {
                    DrawUtils::drawRect(16, index, SCREEN_WIDTH - 16 * 2, 44, 2, (i == (uint32_t) autoboot) ? COLOR_AUTOBOOT : COLOR_BORDER);
                }

                DrawUtils::setFontSize(24);
                DrawUtils::setFontColor((i == (uint32_t) autoboot) ? COLOR_AUTOBOOT : COLOR_TEXT);
                DrawUtils::print(16 * 2, index + 8 + 24, menu_options[i]);
                index += 42 + 8;
            }

            DrawUtils::setFontColor(COLOR_TEXT);

            // draw top bar
            DrawUtils::setFontSize(24);
            DrawUtils::drawPNG(16, 2, icon_png);
            DrawUtils::print(64 + 2, 6 + 24, "Tiramisu Boot Selector");
            DrawUtils::drawRectFilled(8, 8 + 24 + 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);

            // draw bottom bar
            DrawUtils::drawRectFilled(8, SCREEN_HEIGHT - 24 - 8 - 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);
            DrawUtils::setFontSize(18);
            DrawUtils::print(16, SCREEN_HEIGHT - 8, "\ue07d Navigate ");
            DrawUtils::print(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 8, "\ue000 Choose", true);
            const char *autobootHints = "\ue002 Clear Autoboot / \ue003 Select Autoboot";
            DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(autobootHints) / 2, SCREEN_HEIGHT - 8, autobootHints, true);

            DrawUtils::endDraw();

            redraw = false;
        }
    }

    DrawUtils::beginDraw();
    DrawUtils::clear(COLOR_BLACK);
    DrawUtils::endDraw();

    DrawUtils::deinitFont();

    // Call GX2Init to shut down OSScreen
    GX2Init(nullptr);

    free(screenBuffer);

    if (autoboot != autobootOptionInput) {
        writeAutobootOption(configPath, autoboot);
    }

    return selected;
}