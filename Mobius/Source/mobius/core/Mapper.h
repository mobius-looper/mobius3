/**
 * Temporary model mapping functions.
 */

#pragma once

extern class UIEventType* MapEventType(class EventType* src);

extern class ModeDefinition* MapMode(class MobiusMode* mode);

// used only by Project.cpp, redesign
extern void WriteAudio(class Audio* a, const char* path);

// used only by Project.cpp, redesign
extern void WriteFileStub(const char* path, const char* content);
