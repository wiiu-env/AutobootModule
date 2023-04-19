#pragma once

#include <cstdint>
#include <string>

void launchvWiiTitle(uint64_t titleId);

void bootWiiUMenu();

void bootHomebrewLauncher();

void bootWiiuTitle(const std::string& hexId);

void bootvWiiMenu();

void bootHomebrewChannel();

uint64_t getVWiiTitleId(const std::string& hexId);

uint64_t getVWiiHBLTitleId();