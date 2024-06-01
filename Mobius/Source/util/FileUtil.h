/*
 * A file file utilities that wrap Juce file objects
 */

#pragma once

char* ReadFile(const char* path);
bool WriteFile(const char* path, const char* contents);

