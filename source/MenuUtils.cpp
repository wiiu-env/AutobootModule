#include "MenuUtils.h"
#include "ACTAccountInfo.h"
#include "DrawUtils.h"
#include "InputUtils.h"
#include "PairUtils.h"
#include "logger.h"
#include "main.h"
#include "utils.h"
#include "version.h"
#include <coreinit/debug.h>
#include <coreinit/filesystem_fsa.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <cstring>
#include <gx2/state.h>
#include <malloc.h>
#include <memory>
#include <mocha/mocha.h>
#include <sndcore2/core.h>
#include <string>
#include <sysapp/title.h>
#include <vector>

#define AUTOBOOT_MODULE_VERSION "v0.3.1"

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

void drawMenuScreen(const std::map<uint32_t, std::string> &menu, uint32_t selectedIndex, uint32_t autobootIndex, bool updatesBlocked) {
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
    DrawUtils::print(16, 6 + 24, "Boot Selector");
    DrawUtils::drawRectFilled(8, 8 + 24 + 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);
    DrawUtils::setFontSize(16);
    DrawUtils::print(SCREEN_WIDTH - 16, 6 + 24, AUTOBOOT_MODULE_VERSION AUTOBOOT_MODULE_VERSION_EXTRA, true);

    // draw bottom bar
    DrawUtils::drawRectFilled(8, SCREEN_HEIGHT - 24 - 8 - 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);
    DrawUtils::setFontSize(18);
    DrawUtils::print(16, SCREEN_HEIGHT - 8, "\ue07d Navigate ");
    DrawUtils::print(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 8, "\ue000 Choose", true);
    const char *autobootHints = "\ue002/\ue046 Clear Autoboot / \ue003/\ue045 Select Autoboot";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(autobootHints) / 2, SCREEN_HEIGHT - 8, autobootHints, true);

    if (updatesBlocked) {
        DrawUtils::setFontSize(10);
        DrawUtils::print(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 24 - 8 - 4 - 10, "Updates blocked! Hold \ue045 + \ue046 to restore Update folder", true);
    } else {
        DrawUtils::setFontSize(10);
        DrawUtils::print(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 24 - 8 - 4 - 10, "Updates not blocked! Hold \ue045 + \ue046 to delete Update folder", true);
    }

    DrawUtils::endDraw();
}

int32_t handleMenuScreen(std::string &configPath, int32_t autobootOptionInput, const std::map<uint32_t, std::string> &menu) {
    auto screenBuffer = DrawUtils::InitOSScreen();
    if (!screenBuffer) {
        OSFatal("AutobootModule: Failed to alloc memory for screen");
    }

    uint32_t tvBufferSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    DrawUtils::initBuffers(screenBuffer, tvBufferSize, (void *) ((uint32_t) screenBuffer + tvBufferSize), drcBufferSize);
    if (!DrawUtils::initFont()) {
        OSFatal("AutobootModule: Failed to init font");
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

    {
        PairMenu pairMenu;

        int32_t holdUpdateBlockedForFrames = 0;
        while (true) {
            if (pairMenu.ProcessPairScreen()) {
                continue;
            }


            InputUtils::InputData input = InputUtils::getControllerInput();

            if (input.trigger & VPAD_BUTTON_UP) {
                selectedIndex--;

                if (selectedIndex < 0) {
                    selectedIndex = 0;
                }

            } else if (input.trigger & VPAD_BUTTON_DOWN) {
                if (!menu.empty()) {
                    selectedIndex++;

                    if ((uint32_t) selectedIndex >= menu.size()) {
                        selectedIndex = menu.size() - 1;
                    }
                }
            } else if (input.trigger & VPAD_BUTTON_A) {
                break;
            } else if (input.trigger & (VPAD_BUTTON_X | VPAD_BUTTON_MINUS)) {
                autobootIndex = -1;
            } else if (input.trigger & (VPAD_BUTTON_Y | VPAD_BUTTON_PLUS)) {
                autobootIndex = selectedIndex;
            } else if ((input.hold & (VPAD_BUTTON_PLUS | VPAD_BUTTON_MINUS)) == (VPAD_BUTTON_PLUS | VPAD_BUTTON_MINUS)) {
                if (holdUpdateBlockedForFrames++ > 50) {
                    if (gUpdatesBlocked) {
                        gUpdatesBlocked = !RestoreMLCUpdateDirectory();
                    } else {
                        gUpdatesBlocked = DeleteMLCUpdateDirectory();
                    }
                    holdUpdateBlockedForFrames = 0;
                }
            } else {
                holdUpdateBlockedForFrames = 0;
            }

            drawMenuScreen(menu, selectedIndex, autobootIndex, gUpdatesBlocked);
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
        OSFatal("AutobootModule: Failed to alloc memory for screen");
    }

    uint32_t tvBufferSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    DrawUtils::initBuffers(screenBuffer, tvBufferSize, (void *) ((uint32_t) screenBuffer + tvBufferSize), drcBufferSize);
    if (!DrawUtils::initFont()) {
        OSFatal("AutobootModule: Failed to init font");
    }

    int32_t selected = 0;
    {
        PairMenu pairMenu;
        while (true) {
            if (pairMenu.ProcessPairScreen()) {
                continue;
            }

            InputUtils::InputData input = InputUtils::getControllerInput();
            if (input.trigger & VPAD_BUTTON_UP) {
                if (selected > 0) {
                    selected--;
                }
            } else if (input.trigger & VPAD_BUTTON_DOWN) {
                if (selected < (int32_t) data.size() - 1) {
                    selected++;
                }
            } else if (input.trigger & VPAD_BUTTON_A) {
                break;
            }


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

void drawUpdateWarningScreen() {
    DrawUtils::beginDraw();
    DrawUtils::clear(COLOR_BACKGROUND_WARN);

    DrawUtils::setFontColor(COLOR_WARNING);

    // draw top bar
    DrawUtils::setFontSize(48);
    const char *title = "! Warning !";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(title) / 2, 48 + 8, title, true);
    DrawUtils::drawRectFilled(8, 48 + 8 + 16, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);

    DrawUtils::setFontSize(24);

    const char *message = "The update folder currently exists and is not a file.";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(message) / 2, SCREEN_HEIGHT / 2 - 48, message, true);
    message = "Your system might not be blocking updates properly!";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(message) / 2, SCREEN_HEIGHT / 2 - 24, message, true);

    message = "Press \ue002 to block the updates! This can be reverted in the Boot Selector.";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(message) / 2, SCREEN_HEIGHT / 2 + 24, message, true);

    message = "See https://wiiu.hacks.guide/#/block-updates for more information.";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(message) / 2, SCREEN_HEIGHT / 2 + 64 + 24, message, true);

    DrawUtils::setFontSize(16);

    message = "Press the SYNC Button on the Wii U console to connect a controller or GamePad.";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(message) / 2, SCREEN_HEIGHT - 48, message, true);

    // draw bottom bar
    DrawUtils::drawRectFilled(8, SCREEN_HEIGHT - 24 - 8 - 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);
    DrawUtils::setFontSize(18);
    const char *exitHints = "\ue000 Continue without blocking / \ue001 Don't show this again";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(exitHints) / 2, SCREEN_HEIGHT - 8, exitHints, true);

    DrawUtils::endDraw();
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
        OSFatal("AutobootModule: Failed to alloc memory for screen");
    }

    uint32_t tvBufferSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    DrawUtils::initBuffers(screenBuffer, tvBufferSize, (void *) ((uint32_t) screenBuffer + tvBufferSize), drcBufferSize);
    if (!DrawUtils::initFont()) {
        OSFatal("AutobootModule: Failed to init font");
    }

    {
        PairMenu pairMenu;

        while (true) {
            if (pairMenu.ProcessPairScreen()) {
                continue;
            }

            drawUpdateWarningScreen();

            InputUtils::InputData input = InputUtils::getControllerInput();
            if (input.trigger & VPAD_BUTTON_A) {
                break;
            } else if (input.trigger & VPAD_BUTTON_X) {
                gUpdatesBlocked = DeleteMLCUpdateDirectory();
                break;
            } else if (input.trigger & VPAD_BUTTON_B) {
                f = fopen(UPDATE_SKIP_PATH, "w");
                if (f) {
                    // It's **really** important to have this text on the stack.
                    // If it's read from the .rodata section the fwrite will softlock the console because the OSEffectiveToPhysical returns NULL for
                    // everything between 0x00800000 - 0x01000000 at this stage.
                    const char text[] = "If this file exists, the Autoboot Module will not warn you about not blocking updates";
                    fputs(text, f);
                    fclose(f);
                }
                break;
            }
        }
    }

    DrawUtils::beginDraw();
    DrawUtils::clear(COLOR_BLACK);
    DrawUtils::endDraw();

    DrawUtils::deinitFont();

    GX2Init(nullptr);

    free(screenBuffer);
}

void drawDiscInsert(bool wrongDiscInserted) {
    DrawUtils::beginDraw();
    DrawUtils::clear(COLOR_BACKGROUND);
    DrawUtils::setFontColor(COLOR_TEXT);

    DrawUtils::setFontSize(48);

    if (wrongDiscInserted) {
        const char *title = "The disc inserted into the console";
        DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(title) / 2, 40 + 48 + 8, title, true);
        title = "is for a different software title.";
        DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(title) / 2, 40 + 2 * 48 + 8, title, true);
        title = "Please change the disc.";
        DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(title) / 2, 40 + 4 * 48 + 8, title, true);
    } else {
        const char *title = "Please insert a disc.";
        DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(title) / 2, 40 + 48 + 8, title, true);
    }

    DrawUtils::setFontSize(18);
    const char *exitHints = "\ue000 Launch Wii U Menu";
    DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(exitHints) / 2, SCREEN_HEIGHT - 8, exitHints, true);

    DrawUtils::endDraw();
}

bool handleDiscInsertScreen(uint64_t expectedTitleId, uint64_t *titleIdToLaunch) {
    if (titleIdToLaunch == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("titleIdToLaunch is NULL");
        return false;
    }

    if (SYSCheckTitleExists(expectedTitleId)) {
        *titleIdToLaunch = expectedTitleId;
        return true;
    }

    uint64_t titleIdOfDisc = 0;
    bool discInserted;

    uint32_t attempt = 0;
    while (!GetTitleIdOfDisc(&titleIdOfDisc, &discInserted)) {
        if (++attempt > 20) {
            break;
        }
        OSSleepTicks(OSMillisecondsToTicks(100));
    }

    bool wrongDiscInserted = discInserted && (titleIdOfDisc != expectedTitleId);

    if (discInserted && !wrongDiscInserted) {
        *titleIdToLaunch = expectedTitleId;
        return true;
    }

    if (!AXIsInit()) {
        AXInit();
    }

    bool result;
    auto screenBuffer = DrawUtils::InitOSScreen();
    if (!screenBuffer) {
        OSFatal("AutobootModule: Failed to alloc memory for screen");
    }

    uint32_t tvBufferSize  = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    DrawUtils::initBuffers(screenBuffer, tvBufferSize, (void *) ((uint32_t) screenBuffer + tvBufferSize), drcBufferSize);
    if (!DrawUtils::initFont()) {
        OSFatal("AutobootModule: Failed to init font");
    }
    DrawUtils::beginDraw();
    DrawUtils::clear(COLOR_BACKGROUND);
    DrawUtils::endDraw();

    // When an unexpected disc was inserted we need to eject it first.
    bool allowDisc = !wrongDiscInserted;
    {
        PairMenu pairMenu;

        while (true) {
            if (pairMenu.ProcessPairScreen()) {
                continue;
            }

            drawDiscInsert(wrongDiscInserted);

            InputUtils::InputData input = InputUtils::getControllerInput();
            if (input.trigger & VPAD_BUTTON_A) {
                result = false;
                break;
            }


            if (GetTitleIdOfDisc(&titleIdOfDisc, &discInserted)) {
                if (discInserted) {
                    if (!allowDisc) {
                        continue;
                    }
                    *titleIdToLaunch = titleIdOfDisc;
                    DEBUG_FUNCTION_LINE("Disc inserted! %016llX", titleIdOfDisc);
                    result = true;
                    break;
                }
            } else {
                allowDisc = true;
            }
        }
    }

    DrawUtils::beginDraw();
    DrawUtils::clear(COLOR_BLACK);
    DrawUtils::endDraw();

    DrawUtils::deinitFont();

    // Call GX2Init to shut down OSScreen
    GX2Init(nullptr);

    free(screenBuffer);

    return result;
}
