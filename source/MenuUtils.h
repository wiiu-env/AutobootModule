#pragma once

#include "ACTAccountInfo.h"
#include <cstdint>
#include <map>
#include <memory>
#include <nn/act.h>
#include <string>
#include <vector>

#define UPDATE_SKIP_PATH         "fs:/vol/external01/wiiu/environments/skipUpdateWarn"

#define COLOR_WHITE              Color(0xffffffff)
#define COLOR_BLACK              Color(0, 0, 0, 255)
#define COLOR_BACKGROUND         COLOR_BLACK
#define COLOR_BACKGROUND_WARN    Color(255, 40, 0, 255)
#define COLOR_TEXT               COLOR_WHITE
#define COLOR_TEXT2              Color(0xB3ffffff)
#define COLOR_AUTOBOOT           Color(0xaeea00ff)
#define COLOR_BORDER             Color(204, 204, 204, 255)
#define COLOR_BORDER_HIGHLIGHTED Color(0x3478e4ff)

enum {
    BOOT_OPTION_WII_U_MENU,
    BOOT_OPTION_HOMEBREW_LAUNCHER,
    BOOT_OPTION_VWII_SYSTEM_MENU,
    BOOT_OPTION_VWII_HOMEBREW_CHANNEL,
};

int32_t readAutobootOption(std::string &configPath);

void writeAutobootOption(std::string &configPath, int32_t autobootOption);

int32_t handleMenuScreen(std::string &configPath, int32_t autobootOptionInput, const std::map<uint32_t, std::string> &menu);

nn::act::SlotNo handleAccountSelectScreen(const std::vector<std::shared_ptr<AccountInfo>> &data);

void handleUpdateWarningScreen();
