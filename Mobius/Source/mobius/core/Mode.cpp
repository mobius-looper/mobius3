/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static objects representing Mobius operating modes with logic
 * for invoking functions and scheduling events.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "../../util/Util.h"
#include "../../util/List.h"
#include "../../util/Trace.h"

#include "Mobius.h"
#include "../../model/old/MobiusConfig.h"
#include "Mode.h"
#include "Function.h"

/****************************************************************************
 *                                                                          *
 *                              MODE BASE CLASS                             *
 *                                                                          *
 ****************************************************************************/

MobiusMode::MobiusMode()
{
    init();
}

MobiusMode::MobiusMode(const char* name) :
    SystemConstant(name)
{
    init();
}

MobiusMode::MobiusMode(const char* name, const char* display) :
    SystemConstant(name, display)
{
    init();
}

void MobiusMode::init()
{
	minor = false;
	recording = false;
	extends = false;
	rounding = false;
	altFeedbackSensitive = false;
	altFeedbackDisabled = false;
    invokeHandler = false;
}

MobiusMode::~MobiusMode()
{
}

/****************************************************************************
 *                                                                          *
 *   							 ENUMERATION                                *
 *                                                                          *
 ****************************************************************************/

/*
 * Originally used static arrays of MobiusMode object pointers, but once
 * we started moving these into function-specific files, this
 * wasn't reliable because the static objects did not always
 * exist when this array was initialized.
 *
 * Instead, we'll build the arrays at runtime.   Before doing any searches
 * on static functions, Mobius needs to call MobiusMode::initModes()
 */

#define MAX_MODES 50

MobiusMode* Modes[MAX_MODES];
int ModeIndex = 0;

void add(MobiusMode* mode)
{
    if (mode == nullptr) {
        // must be a link problem
        printf("Static mode object not initialized!\n");
        fflush(stdout);
    }
	else if (ModeIndex >= MAX_MODES - 1) {
		printf("Static mode array overflow!\n");
		fflush(stdout);
	}
	else {
		Modes[ModeIndex++] = mode;
		// keep it nullptr terminated
		Modes[ModeIndex] = nullptr;
	}
}

/**
 * Called early during Mobius initializations to initialize the 
 * static mode arrays. This never changes once initialized.
 */
void MobiusMode::initModes()
{
    // ignore if already initialized
    if (ModeIndex == 0) {
        add(ResetMode);
        add(RunMode);
        add(PlayMode);
        add(RecordMode);
        add(ThresholdMode);
        add(OverdubMode);
        add(MultiplyMode);
        add(InsertMode);
        add(StutterMode);
        add(RehearseMode);
        add(RehearseRecordMode);
        add(ReplaceMode);
        add(SubstituteMode);
        add(MuteMode);
        add(ConfirmMode);
        add(SwitchMode);
        add(SynchronizeMode);
        add(PauseMode);

        // minor modes
        add(ReverseMode);
        add(PitchOctaveMode);
        add(PitchStepMode);
        add(PitchBendMode);
        add(SpeedOctaveMode);
        add(SpeedStepMode);
        add(SpeedBendMode);
        add(SpeedToggleMode);
        add(TimeStretchMode);
        add(CaptureMode);
        add(SoloMode);
        add(GlobalMuteMode);
        add(GlobalPauseMode);
        add(WindowMode);
    }
};

MobiusMode** MobiusMode::getModes()
{
	return Modes;
}

/**
 * Search for a mode by name or display name.
 */
MobiusMode* MobiusMode::getMode(const char* name) 
{
	MobiusMode* found = nullptr;
	if (name != nullptr) {
		for (int i = 0 ; Modes[i] != nullptr ; i++) {
			MobiusMode* m = Modes[i];
			if (StringEqualNoCase(name, m->getName()) ||
				StringEqualNoCase(name, m->getDisplayName())) {
				found = m;
				break;
			}
		}
	}
	return found;
}

/**
 * Set the mode display names from a message catalog.
 */
#if 0
void MobiusMode::localizeAll(MessageCatalog* cat)
{
	for (int i = 0 ; Modes[i] != nullptr ; i++) {
		MobiusMode* mode = Modes[i];
        mode->localize(cat);
	}
}
#endif

/**
 * Check the global configuration and update some of the mode options.
 *
 * NOTE: This is currently stubbed out in Mobius.cpp 
 */
void MobiusMode::updateConfiguration(MobiusConfig* config)
{
	StringList* names = config->getAltFeedbackDisables();
	
	// initialize
	for (int i = 0 ; Modes[i] != nullptr ; i++)
	  Modes[i]->altFeedbackDisabled = false;

	if (names != nullptr) {
		for (int i = 0 ; Modes[i] != nullptr ; i++) {
			MobiusMode* m = Modes[i];
			if (m->altFeedbackSensitive)
			  m->altFeedbackDisabled = names->contains(m->getName());
		}
	}
}

/****************************************************************************
 *                                                                          *
 *                                 INVOCATION                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Should only be here if we declare invokeHandler in which case
 * the method must be overloaded.
 */
void MobiusMode::invoke(Action* action, Loop* loop)
{
    (void)action;
    (void)loop;
    Trace(1, "MobiusMode::invoke should have been overloaded!\n");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
