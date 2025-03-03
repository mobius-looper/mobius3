/**
 * Base model for major operating modes within the engine.
 * These are part of the MobiusState model.
 * A track will always be in one of the major modes and
 * may be in zero or more minor modes.
 *
 * Need different constant object pointer names to avoid conflicts
 * with core.  Need to start being consistent and use the UI prefix
 * for both classes and pointer names.
 *
 * update: need for this is dimished after MobiusViewer
 */

#pragma once

#include <vector>
#include "SystemConstant.h"

class ModeDefinition : public SystemConstant
{
  public:

	ModeDefinition(const char* name);
	virtual ~ModeDefinition();

    int ordinal;				// internal number for indexing

    //////////////////////////////////////////////////////////////////////
    // Global Registry
    //////////////////////////////////////////////////////////////////////

    static std::vector<ModeDefinition*> Instances;
	static ModeDefinition* find(const char* name);
    static void trace();

};

// Major modes

extern ModeDefinition* UIGlobalResetMode;

extern ModeDefinition* UIConfirmMode;
extern ModeDefinition* UIInsertMode;
extern ModeDefinition* UIMultiplyMode;
extern ModeDefinition* UIMuteMode;
extern ModeDefinition* UIOverdubMode;
extern ModeDefinition* UIPauseMode;
extern ModeDefinition* UIPlayMode;
extern ModeDefinition* UIRecordMode;
extern ModeDefinition* UIRehearseMode;
extern ModeDefinition* UIRehearseRecordMode;
extern ModeDefinition* UIReplaceMode;
extern ModeDefinition* UIResetMode;
extern ModeDefinition* UIRunMode;
extern ModeDefinition* UIStutterMode;
extern ModeDefinition* UISubstituteMode;
extern ModeDefinition* UISwitchMode;
extern ModeDefinition* UISynchronizeMode;
extern ModeDefinition* UIThresholdMode;


// Minor modes

extern ModeDefinition* UICaptureMode;
extern ModeDefinition* UIGlobalMuteMode;
extern ModeDefinition* UIGlobalPauseMode;
extern ModeDefinition* UIHalfSpeedMode;
extern ModeDefinition* UIPitchOctaveMode;
extern ModeDefinition* UIPitchStepMode;
extern ModeDefinition* UIPitchBendMode;
extern ModeDefinition* UISpeedOctaveMode;
extern ModeDefinition* UISpeedStepMode;
extern ModeDefinition* UISpeedBendMode;
extern ModeDefinition* UISpeedToggleMode;
extern ModeDefinition* UITimeStretchMode;
extern ModeDefinition* UIReverseMode;
extern ModeDefinition* UISoloMode;
extern ModeDefinition* UIWindowMode;
