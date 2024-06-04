/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Functions for capturing the audio stream and saving to a file.
 * These are all global functions.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "../Action.h"
#include "../Event.h"
#include "../Function.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../Mobius.h"
#include "../Mode.h"

//////////////////////////////////////////////////////////////////////
//
// CaptureMode
//
//////////////////////////////////////////////////////////////////////

class CaptureModeType : public MobiusMode {
  public:
	CaptureModeType();
};

CaptureModeType::CaptureModeType() : 
    MobiusMode("capture", "Capture")
{
	minor = true;
}

CaptureModeType CaptureModeObj;
MobiusMode* CaptureMode = &CaptureModeObj;

//////////////////////////////////////////////////////////////////////
//
// Capture Functions
//
//////////////////////////////////////////////////////////////////////

class CaptureFunction : public Function {
  public:
	CaptureFunction(bool stop, bool save);
	void invoke(Action* action, Mobius* m);
  private:
	bool mStop;
	bool mSave;
};

CaptureFunction StartCaptureObj {false, false};
Function* StartCapture = &StartCaptureObj;

CaptureFunction StopCaptureObj {true, false};
Function* StopCapture = &StopCaptureObj;

CaptureFunction SaveCaptureObj {false, true};
Function* SaveCapture = &SaveCaptureObj;

CaptureFunction::CaptureFunction(bool stop, bool save)
{
	global = true;
	mStop = stop;
	mSave = save;

	if (mSave) {
		setName("SaveCapture");
        alias1 = "SaveAudioRecording";
	}
	else if (mStop) {
		setName("StopCapture");
		alias1 = "StopAudioRecording";
	}
	else {
		setName("StartCapture");
		alias1 = "StartAudioRecording";
	}
}

void CaptureFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);
		if (mSave) 
		  m->saveCapture(action);
		else if (mStop)
		  m->stopCapture(action);
		else
		  m->startCapture(action);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
