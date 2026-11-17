#pragma once

#include <cstdint>

void bootWiiUMenu();

void bootHomebrewLauncher();

void bootvWiiMenu();

void bootHomebrewChannel();

uint64_t getVWiiHBLTitleId();

// vWii titles start with 0x0001, Wii U titles with 0x0005.
bool isVWiiTitle(uint64_t titleId);

bool isWiiUTitle(uint64_t titleId);

bool vWiiTitleExists(uint64_t titleId);

bool titleExists(uint64_t titleId);

// Launches a vWii or Wii U title; the title id decides which.
void bootCustomTitle(uint64_t titleId);
