#pragma once

#include "ACTAccountInfo.h"
#include <array>
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

struct BootOption {
    std::string title;
    std::string hexId;
    bool vWii;
};

enum {
    BOOT_OPTION_WII_U_MENU = 0,
    BOOT_OPTION_HOMEBREW_LAUNCHER,
    BOOT_OPTION_VWII_SYSTEM_MENU,
    BOOT_OPTION_VWII_HOMEBREW_CHANNEL,

    BOOT_OPTION_MAX_OPTIONS
};

constexpr std::array<const char*, BOOT_OPTION_MAX_OPTIONS> autoboot_base_config_strings{
    "wiiu_menu",
    "homebrew_launcher",
    "vwii_system_menu",
    "vwii_homebrew_channel",
};

extern std::vector<std::string> autoboot_config_strings;
extern std::vector<BootOption> custom_boot_options;

void readBootOptionsFromSD(const std::string &configPath);

int32_t readAutobootOption(const std::string &configPath);

void writeAutobootOption(const std::string &configPath, int32_t autobootOption);

int32_t handleMenuScreen(const std::string &configPath, int32_t autobootOptionInput, const std::map<uint32_t, std::string> &menu);

nn::act::SlotNo handleAccountSelectScreen(const std::vector<std::shared_ptr<AccountInfo>> &data);

void handleUpdateWarningScreen();
