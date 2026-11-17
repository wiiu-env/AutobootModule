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
#define COLOR_BACKGROUND_WARN    Color(255, 251, 4, 255)
#define COLOR_TEXT               COLOR_WHITE
#define COLOR_WARNING            COLOR_BLACK
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

// Entries from bootOptions.cfg use BOOT_OPTION_CUSTOM_BASE + their index in the file.
#define BOOT_OPTION_CUSTOM_BASE 0x100
// At most this many custom entries, fewer if the menu runs out of rows.
#define MAX_CUSTOM_BOOT_OPTIONS 4
// The menu doesn't scroll, and an eighth row would cover the update status line.
#define MAX_MENU_ROWS           7

struct CustomBootOption {
    std::string name;
    uint64_t titleId;
};

std::vector<CustomBootOption> readCustomBootOptions(const std::string &path);

int32_t readAutobootOption(std::string &configPath, const std::vector<CustomBootOption> &customOptions);

void writeAutobootOption(std::string &configPath, int32_t autobootOption, const std::vector<CustomBootOption> &customOptions);

int32_t handleMenuScreen(std::string &configPath, int32_t autobootOptionInput, const std::map<uint32_t, std::string> &menu,
                         const std::vector<CustomBootOption> &customOptions);

nn::act::SlotNo handleAccountSelectScreen(const std::vector<std::shared_ptr<AccountInfo>> &data);

void handleUpdateWarningScreen();

bool handleDiscInsertScreen(uint64_t expectedTitleId, uint64_t *titleIdToLaunch);
