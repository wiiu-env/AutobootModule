#include "MenuUtils.h"
#include "ACTAccountInfo.h"
#include "DrawUtils.h"
#include "icon_png.h"
#include "logger.h"
#include "version.h"
#include <coreinit/debug.h>
#include <coreinit/screen.h>
#include <cstring>
#include <gx2/state.h>
#include <malloc.h>
#include <memory>
#include <nn/act/client_cpp.h>
#include <string>
#include <vector>
#include <vpad/input.h>

#define AUTOBOOT_MODULE_VERSION "v0.1.1"

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

int32_t handleMenuScreen(std::string &configPath, int32_t autobootOptionInput, const std::map<uint32_t, std::string> &menu) {
    auto screenBuffer = DrawUtils::InitOSScreen();
    if (!screenBuffer) {
        OSFatal("Failed to alloc memory for screen");
    }

    uint32_t tvBufferSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    DrawUtils::initBuffers(screenBuffer, tvBufferSize, (void *) ((uint32_t) screenBuffer + tvBufferSize), drcBufferSize);
    if (!DrawUtils::initFont()) {
        OSFatal("Failed to init font");
    }

    int32_t selectedIndex = autobootOptionInput > 0 ? autobootOptionInput : 0;
    int autobootIndex     = autobootOptionInput;

    // "Convert" id to index
    int32_t offset = 0;
    for (auto &item : menu) {
        if ((uint32_t) selectedIndex == item.first) {
            selectedIndex = offset;
            break;
        }
        offset++;
    }
    if (autobootIndex > 0) {
        offset = 0;
        for (auto &item : menu) {
            if ((uint32_t) autobootIndex == item.first) {
                autobootIndex = offset;
                break;
            }
            offset++;
        }
    }

    bool redraw = true;
    while (true) {
        VPADStatus vpad{};
        VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);

        if (vpad.trigger & VPAD_BUTTON_UP) {
            selectedIndex--;

            if (selectedIndex < 0) {
                selectedIndex = 0;
            }

            redraw = true;
        } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
            if (!menu.empty()) {
                selectedIndex++;

                if ((uint32_t) selectedIndex >= menu.size()) {
                    selectedIndex = menu.size() - 1;
                }
                redraw = true;
            }
        } else if (vpad.trigger & VPAD_BUTTON_A) {
            break;
        } else if (vpad.trigger & VPAD_BUTTON_X) {
            autobootIndex = -1;
            redraw        = true;
        } else if (vpad.trigger & VPAD_BUTTON_Y) {
            autobootIndex = selectedIndex;
            redraw        = true;
        }

        if (redraw) {
            DrawUtils::beginDraw();
            DrawUtils::clear(COLOR_BACKGROUND);

            // draw buttons
            uint32_t index = 8 + 24 + 8 + 4;
            for (uint32_t i = 0; i < menu.size(); i++) {
                if (i == (uint32_t) selectedIndex) {
                    DrawUtils::drawRect(16, index, SCREEN_WIDTH - 16 * 2, 44, 4, COLOR_BORDER_HIGHLIGHTED);
                } else {
                    DrawUtils::drawRect(16, index, SCREEN_WIDTH - 16 * 2, 44, 2, (i == (uint32_t) autobootIndex) ? COLOR_AUTOBOOT : COLOR_BORDER);
                }

                std::string curName = std::next(menu.begin(), i)->second;

                DrawUtils::setFontSize(24);
                DrawUtils::setFontColor((i == (uint32_t) autobootIndex) ? COLOR_AUTOBOOT : COLOR_TEXT);
                DrawUtils::print(16 * 2, index + 8 + 24, curName.c_str());
                index += 42 + 8;
            }

            DrawUtils::setFontColor(COLOR_TEXT);

            // draw top bar
            DrawUtils::setFontSize(24);
            DrawUtils::drawPNG(16, 2, icon_png);
            DrawUtils::print(64 + 2, 6 + 24, "Boot Selector");
            DrawUtils::drawRectFilled(8, 8 + 24 + 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);
            DrawUtils::setFontSize(16);
            DrawUtils::print(SCREEN_WIDTH - 16, 6 + 24, AUTOBOOT_MODULE_VERSION AUTOBOOT_MODULE_VERSION_EXTRA, true);

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

    int32_t selected = -1;
    int32_t autoboot = -1;
    // convert index to key
    if (selectedIndex >= 0 && (uint32_t) selectedIndex < menu.size()) {
        selected = std::next(menu.begin(), selectedIndex)->first;
    }
    if (autobootIndex >= 0 && (uint32_t) autobootIndex < menu.size()) {
        autoboot = std::next(menu.begin(), autobootIndex)->first;
    }

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
    if (!DrawUtils::initFont()) {
        OSFatal("Failed to init font");
    }

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

void handleUpdateWarningScreen() {
    FILE *f = fopen(UPDATE_SKIP_PATH, "r");
    if (f) {
        DEBUG_FUNCTION_LINE("Skipping update warning screen");
        fclose(f);
        return;
    }

    auto screenBuffer = DrawUtils::InitOSScreen();
    if (!screenBuffer) {
        OSFatal("Failed to alloc memory for screen");
    }

    uint32_t tvBufferSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    DrawUtils::initBuffers(screenBuffer, tvBufferSize, (void *) ((uint32_t) screenBuffer + tvBufferSize), drcBufferSize);
    if (!DrawUtils::initFont()) {
        OSFatal("Failed to init font");
    }

    DrawUtils::beginDraw();
    DrawUtils::clear(COLOR_BACKGROUND_WARN);

    DrawUtils::setFontColor(COLOR_TEXT);

    // draw top bar
    DrawUtils::setFontSize(48);
    const char *title = "! Warning !";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(title) / 2, 48 + 8, title, true);
    DrawUtils::drawRectFilled(8, 48 + 8 + 16, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);

    DrawUtils::setFontSize(24);

    const char *message = "The update folder currently exists and is not a file.";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(message) / 2, SCREEN_HEIGHT / 2 - 24, message, true);
    message = "Your system might not be blocking updates properly!";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(message) / 2, SCREEN_HEIGHT / 2 + 0, message, true);
    message = "See https://wiiu.hacks.guide/#/block-updates for more information.";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(message) / 2, SCREEN_HEIGHT / 2 + 24, message, true);

    // draw bottom bar
    DrawUtils::drawRectFilled(8, SCREEN_HEIGHT - 24 - 8 - 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);
    DrawUtils::setFontSize(18);
    const char *exitHints = "\ue000 Continue / \ue001 Don't show this again";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(exitHints) / 2, SCREEN_HEIGHT - 8, exitHints, true);

    DrawUtils::endDraw();

    while (true) {
        VPADStatus vpad{};
        VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);

        if (vpad.trigger & VPAD_BUTTON_A) {
            break;
        } else if (vpad.trigger & VPAD_BUTTON_B) {
            f = fopen(UPDATE_SKIP_PATH, "w");
            if (f) {
                fputs("If this file exists, the Autoboot Module will not warn you about not blocking updates", f);
                fclose(f);
            }
            break;
        }
    }

    DrawUtils::clear(COLOR_BLACK);
    DrawUtils::endDraw();

    DrawUtils::deinitFont();

    free(screenBuffer);
}
