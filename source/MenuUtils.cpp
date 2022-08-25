#include "MenuUtils.h"
#include "DrawUtils.h"
#include <cstdint>
#include <cstring>
#include <malloc.h>
#include <string>
#include <vector>

#include <coreinit/debug.h>
#include <coreinit/screen.h>
#include <gx2/state.h>
#include <memory>
#include <nn/act/client_cpp.h>
#include <vpad/input.h>

#include "ACTAccountInfo.h"
#include "icon_png.h"
#include "logger.h"

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

template<typename... Args>
std::string string_format(const std::string &format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    auto size  = static_cast<size_t>(size_s);
    auto buf   = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

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

int32_t handleMenuScreen(std::string &configPath, int32_t autobootOptionInput, bool showHBL) {
    auto screenBuffer = DrawUtils::InitOSScreen();
    if (!screenBuffer) {
        OSFatal("Failed to alloc memory for screen");
    }

    uint32_t tvBufferSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    DrawUtils::initBuffers(screenBuffer, tvBufferSize, (void *) ((uint32_t) screenBuffer + tvBufferSize), drcBufferSize);
    DrawUtils::initFont();

    uint32_t selected = autobootOptionInput > 0 ? autobootOptionInput : 0;
    int autoboot      = autobootOptionInput;
    bool redraw       = true;
    while (true) {
        VPADStatus vpad{};
        VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);

        if (vpad.trigger & VPAD_BUTTON_UP) {
            if (selected > 0) {
                selected--;
                if (!showHBL && selected == BOOT_OPTION_HOMEBREW_LAUNCHER) {
                    selected--;
                }

                redraw = true;
            }
        } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
            if (selected < sizeof(menu_options) / sizeof(char *) - 1) {
                selected++;
                if (!showHBL && selected == BOOT_OPTION_HOMEBREW_LAUNCHER) {
                    selected++;
                }
                redraw = true;
            }
        } else if (vpad.trigger & VPAD_BUTTON_A) {
            break;
        } else if (vpad.trigger & VPAD_BUTTON_X) {
            autoboot = -1;
            redraw   = true;
        } else if (vpad.trigger & VPAD_BUTTON_Y) {
            autoboot = selected;
            redraw   = true;
        }

        if (redraw) {
            DrawUtils::beginDraw();
            DrawUtils::clear(COLOR_BACKGROUND);

            // draw buttons
            uint32_t index = 8 + 24 + 8 + 4;
            for (uint32_t i = 0; i < sizeof(menu_options) / sizeof(char *); i++) {
                if (!showHBL && i == BOOT_OPTION_HOMEBREW_LAUNCHER) {
                    continue;
                }
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


nn::act::SlotNo handleAccountSelectScreen(const std::vector<std::shared_ptr<AccountInfo>> &data) {
    auto screenBuffer = DrawUtils::InitOSScreen();
    if (!screenBuffer) {
        OSFatal("Failed to alloc memory for screen");
    }

    uint32_t tvBufferSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    DrawUtils::initBuffers(screenBuffer, tvBufferSize, (void *) ((uint32_t) screenBuffer + tvBufferSize), drcBufferSize);
    DrawUtils::initFont();

    int32_t selected = 0;
    bool redraw      = true;
    while (true) {
        VPADStatus vpad{};
        VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);

        if (vpad.trigger & VPAD_BUTTON_UP) {
            if (selected > 0) {
                selected--;
                redraw = true;
            }
        } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
            if (selected < (int32_t) data.size() - 1) {
                selected++;
                redraw = true;
            }
        } else if (vpad.trigger & VPAD_BUTTON_A) {
            break;
        }

        if (redraw) {
            DrawUtils::beginDraw();
            DrawUtils::clear(COLOR_BACKGROUND);

            // draw buttons
            uint32_t index = 8 + 24 + 8 + 4;
            int32_t start  = (selected / 5) * 5;
            int32_t end    = (start + 5) < (int32_t) data.size() ? (start + 5) : data.size();
            for (int i = start; i < end; i++) {
                auto &val = data[i];
                if (val->miiImageSize > 0) {
                    // Draw Mii
                    auto width         = 128;
                    auto height        = 128;
                    auto target_height = 64u;
                    auto target_width  = 64u;
                    auto xOffset       = 20;
                    auto yOffset       = index;
                    for (uint32_t y = 0; y < target_height; y++) {
                        for (uint32_t x = 0; x < target_width; x++) {
                            uint32_t col    = (((x) *width / target_width) + ((target_height - y - 1) * height / target_height) * width) * 4;
                            uint32_t colVal = ((uint32_t *) &val->miiImageBuffer[col + 1])[0];
                            if (colVal == 0x00808080) { // Remove the green background.
                                DrawUtils::drawPixel(x + xOffset, y + yOffset, COLOR_BACKGROUND.r, COLOR_BACKGROUND.g, COLOR_BACKGROUND.b, COLOR_BACKGROUND.a);
                            } else {
                                DrawUtils::drawPixel(x + xOffset, y + yOffset, val->miiImageBuffer[col + 1], val->miiImageBuffer[col + 2], val->miiImageBuffer[col + 3], val->miiImageBuffer[col]);
                            }
                        }
                    }
                }

                if (i == selected) {
                    DrawUtils::drawRect(16, index, SCREEN_WIDTH - 16 * 2, 64, 4, COLOR_BORDER_HIGHLIGHTED);
                }

                DrawUtils::setFontSize(24);
                DrawUtils::setFontColor(COLOR_TEXT);

                std::string finalStr = val->name + (val->isNetworkAccount ? (std::string(" (NNID: ") + val->accountId + ")") : "");
                DrawUtils::print(72 + 16 * 2, index + 8 + 32, finalStr.c_str());

                index += 72 + 8;
            }

            DrawUtils::setFontColor(COLOR_TEXT);

            // draw top bar
            DrawUtils::setFontSize(24);
            DrawUtils::print(16, 6 + 24, "Select your Account");
            auto curPage    = (selected / 5) + 1;
            auto totalPages = data.size() % 5 == 0 ? data.size() / 5 : data.size() / 5 + 1;
            DrawUtils::print(SCREEN_WIDTH - 50, 6 + 24, string_format("%d/%d", curPage, totalPages).c_str());
            DrawUtils::drawRectFilled(8, 8 + 24 + 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);

            // draw bottom bar
            DrawUtils::drawRectFilled(8, SCREEN_HEIGHT - 24 - 8 - 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);
            DrawUtils::setFontSize(18);
            DrawUtils::print(16, SCREEN_HEIGHT - 8, "\ue07d Navigate ");
            DrawUtils::print(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 8, "\ue000 Choose", true);

            if (start > 0) {
                DrawUtils::setFontSize(36);
                DrawUtils::print(SCREEN_WIDTH - 30, 68, "\uE01B", true);
            }

            if (end < (int32_t) data.size()) {
                DrawUtils::setFontSize(36);
                DrawUtils::print(SCREEN_WIDTH - 30, SCREEN_HEIGHT - 40, "\uE01C", true);
            }

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

    auto i                     = 0;
    nn::act::SlotNo resultSlot = 0;
    for (auto const &val : data) {
        if (i == selected) {
            resultSlot = val->slot;
        }
        i++;
    }

    return resultSlot;
}