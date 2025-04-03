/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Replace a loop section while hearing the current content.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "../../../util/Util.h"
#include "../../../model/ParameterConstants.h"
#include "../../../model/SymbolId.h"
#include "../../../model/TrackState.h"

#include "../Action.h"
#include "../Event.h"
#include "../Function.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../Mode.h"

//////////////////////////////////////////////////////////////////////
//
// SubstituteMode
//
//////////////////////////////////////////////////////////////////////

class SubstituteModeType : public MobiusMode {
  public:
	SubstituteModeType();
};

SubstituteModeType::SubstituteModeType() :
    MobiusMode("substitute")
{
	recording = true;
	altFeedbackSensitive = true;
    stateMode = TrackState::ModeSubstitute;
}

SubstituteModeType SubstituteModeObj;
MobiusMode* SubstituteMode = &SubstituteModeObj;

//////////////////////////////////////////////////////////////////////
//
// SubstituteEvent
//
//////////////////////////////////////////////////////////////////////

class SubstituteEventType : public EventType {
  public:
    virtual ~SubstituteEventType() {}
	SubstituteEventType();
};

SubstituteEventType::SubstituteEventType()
{
	name = "Substitute";
}

SubstituteEventType SubstituteEventObj;
EventType* SubstituteEvent = &SubstituteEventObj;

//////////////////////////////////////////////////////////////////////
//
// SubstituteFunction
//
//////////////////////////////////////////////////////////////////////

class SubstituteFunction : public Function {
  public:
	SubstituteFunction(bool sus);
	bool isSustain();
	void doEvent(Loop* l, Event* e);
};

// SUS first for longFunction
SubstituteFunction SUSSubstituteObj {true};
Function* SUSSubstitute = &SUSSubstituteObj;

SubstituteFunction SubstituteObj {false};
Function* Substitute = &SubstituteObj;

SubstituteFunction::SubstituteFunction(bool sus)
{
	eventType = SubstituteEvent;
    mMode = SubstituteMode;
	majorMode = true;
	mayCancelMute = true;
	quantized = true;
	cancelReturn = true;
	sustain = sus;

	// could do SoundCopy then enter Substitute?
	//switchStack = true;
	//switchStackMutex = true;

	if (!sus) {
		setName("Substitute");
		//longFunction = SUSSubstitute;
        // formarly could also force this with SustainFunctions parameter
        //maySustain = true;
        // not sure why this was, the whole concept of what might act
        // as a confirmation needs thought
        //mayConfirm = true;
        symbol = FuncSubstitute;
	}
	else {
		setName("SUSSubstitute");
        symbol = FuncSUSSubstitute;
	}
}

bool SubstituteFunction::isSustain()
{
    return sustain;
}

/**
 * SubstituteEvent event handler.
 * Like Replace except the original loop is audible.
 */
void SubstituteFunction::doEvent(Loop* loop, Event* event)
{
	MobiusMode* mode = loop->getMode();

	if (mode == SubstituteMode) {
        loop->finishRecording(event);
    }
    else {
        if (mode == RehearseMode)
          loop->cancelRehearse(event);
        else if (loop->isRecording())
          loop->finishRecording(event);

        loop->cancelPrePlay();
		loop->checkMuteCancel(event);

        loop->setRecording(true);
        loop->setMode(SubstituteMode);
    }

	loop->validate(event);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
