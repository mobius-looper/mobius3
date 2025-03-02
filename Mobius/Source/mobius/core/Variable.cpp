// stubbed out various file oriented variables
// I'm really HATING these, why was this necessary?  just to give
// test scripts something to ponder about?  It is extremely
// tedious maintaining all this, it should be generated at the
// very least

/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Mobius Variables.
 *
 * These are sort of like Parameters except they are typically read-only
 * and accessible only in scripts.
 *
 * A few things are represented as both variables and parameters
 * (LoopFrames, LoopCycles).
 *
 * I'm leaning toward moving most of the read-only "track parameters"
 * from being ParameterDefs to script variables.  They're easier to 
 * maintain and they're really only for use in scripts anyway.
 */

#include <stdio.h>
#include <memory.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../util/Vbuf.h"
#include "../../model/Trigger.h"
#include "../../model/SyncConstants.h"

// for AUDIO_FRAMES_PER_BUFFER
#include "AudioConstants.h"

// for getLastSampleFrames
#include "../MobiusKernel.h"
#include "../sync/SyncMaster.h"

#include "Event.h"
#include "EventManager.h"
#include "Expr.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "Mode.h"
#include "Project.h"
#include "Script.h"
#include "ScriptInterpreter.h"
#include "Synchronizer.h"
//#include "SyncState.h"
//#include "SyncTracker.h"
#include "Track.h"
#include "ParameterSource.h"

#include "Variable.h"
#include "Mem.h"

/****************************************************************************
 *                                                                          *
 *   						  INTERNAL VARIABLE                             *
 *                                                                          *
 ****************************************************************************/

ScriptInternalVariable::ScriptInternalVariable()
{
    mName = nullptr;
	mAlias = nullptr;
	//mType = TYPE_INT;
}

ScriptInternalVariable::~ScriptInternalVariable()
{
    delete mName;
	delete mAlias;
}

void ScriptInternalVariable::setName(const char* name)
{
    delete mName;
    mName = CopyString(name);
}

const char* ScriptInternalVariable::getName()
{
    return mName;
}

void ScriptInternalVariable::setAlias(const char* name)
{
    delete mAlias;
    mAlias = CopyString(name);
}

/**
 * Compare the external name against the name and the alias.
 * Kludge to handle renames of a few variables.  Should support
 * multiple aliases.
 */
bool ScriptInternalVariable::isMatch(const char* name)
{
	return (StringEqualNoCase(name, mName) ||
			StringEqualNoCase(name, mAlias));
}

/**
 * The base implementation of getValue.  
 * We almost always forward this so the active track, but in a few cases
 * it will be overloaded to extract information from the interpreter.
 */
void ScriptInternalVariable::getValue(ScriptInterpreter* si, 	
											 ExValue* value)
{
	getTrackValue(si->getTargetTrack(), value);
}

void ScriptInternalVariable::getTrackValue(Track* t, ExValue* value)
{
    (void)t;
	value->setInt(0);
}

/**
 * Verify few variables can be set, the ones that can are usually
 * just for unit tests and debugging.
 */
void ScriptInternalVariable::setValue(ScriptInterpreter* si, 
											 ExValue* value)
{
    (void)si;
    (void)value;
	Trace(1, "Attempt to set script internal variable %s\n", mName);
}

/****************************************************************************
 *                                                                          *
 *   						SCRIPT EXECUTION STATE                          *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// sustainCount
//
// Number of times the script has been notified of a sustain.
//
//////////////////////////////////////////////////////////////////////

class SustainCountVariableType : public ScriptInternalVariable {
  public:
    virtual ~SustainCountVariableType() {}
    SustainCountVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

SustainCountVariableType::SustainCountVariableType()
{
    setName("sustainCount");
}

void SustainCountVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getSustainCount());
}

SustainCountVariableType SustainCountVariableObj;
ScriptInternalVariable* SustainCountVariable = &SustainCountVariableObj;

//////////////////////////////////////////////////////////////////////
//
// clickCount
//
// Number of times the script has been reentred due to multi-clicks.
//
//////////////////////////////////////////////////////////////////////

class ClickCountVariableType : public ScriptInternalVariable {
  public:
    virtual ~ClickCountVariableType() {}
    ClickCountVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

ClickCountVariableType::ClickCountVariableType()
{
    setName("clickCount");
}

void ClickCountVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getClickCount());
}

ClickCountVariableType ClickCountVariableObj;
ScriptInternalVariable* ClickCountVariable = &ClickCountVariableObj;

//////////////////////////////////////////////////////////////////////
//
// triggerSource
//
// The source of the trigger.  Originally this was the name
// of a FunctionSource enumeration item, now it is the name of
// a Trigger constant.
//
//////////////////////////////////////////////////////////////////////

class TriggerSourceValueVariableType : public ScriptInternalVariable {
  public:
    virtual ~TriggerSourceValueVariableType() {}
    TriggerSourceValueVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

TriggerSourceValueVariableType::TriggerSourceValueVariableType()
{
    setName("triggerSource");
}

void TriggerSourceValueVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	Trigger* t = si->getTrigger();
    if (t != nullptr)
      value->setString(t->getName());
    else
      value->setNull();
}

TriggerSourceValueVariableType TriggerSourceValueVariableObj;
ScriptInternalVariable* TriggerSourceValueVariable = &TriggerSourceValueVariableObj;

//////////////////////////////////////////////////////////////////////
//
// triggerNumber
//
// The unique id of the trigger.  For TriggerMidi this will
// be a combination of the MIDI status, channel, and number.  For other
// sources it will be a key code or other simple number.
//
//////////////////////////////////////////////////////////////////////

class TriggerNumberVariableType : public ScriptInternalVariable {
  public:
    virtual ~TriggerNumberVariableType() {}
    TriggerNumberVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

TriggerNumberVariableType::TriggerNumberVariableType()
{
    setName("triggerNumber");
}

void TriggerNumberVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getTriggerId());
}

TriggerNumberVariableType TriggerNumberVariableObj;
ScriptInternalVariable* TriggerNumberVariable = &TriggerNumberVariableObj;

//////////////////////////////////////////////////////////////////////
//
// triggerValue, triggerVelocity
//
// An optional extra value associated with the trigger.
// For MIDI triggers this will be the second byte, the note velocity
// for notes or the controller value for controllers.
//
//////////////////////////////////////////////////////////////////////

class TriggerValueVariableType : public ScriptInternalVariable {
  public:
    virtual ~TriggerValueVariableType() {}
    TriggerValueVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

TriggerValueVariableType::TriggerValueVariableType()
{
    setName("triggerValue");
    setAlias("triggerVelocity");
}

void TriggerValueVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getTriggerValue());
}

TriggerValueVariableType TriggerValueVariableObj;
ScriptInternalVariable* TriggerValueVariable = &TriggerValueVariableObj;

//////////////////////////////////////////////////////////////////////
//
// triggerOffset
//
// An optional extra value associated with the spread functions.
// This will have the relative position of the trigger from the
// center of the range.
//
//////////////////////////////////////////////////////////////////////

class TriggerOffsetVariableType : public ScriptInternalVariable {
  public:
    virtual ~TriggerOffsetVariableType() {}
    TriggerOffsetVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

TriggerOffsetVariableType::TriggerOffsetVariableType()
{
    setName("triggerOffset");
}

void TriggerOffsetVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getTriggerOffset());
}

TriggerOffsetVariableType TriggerOffsetVariableObj;
ScriptInternalVariable* TriggerOffsetVariable = &TriggerOffsetVariableObj;

//////////////////////////////////////////////////////////////////////
//
// midiType
//
// The type of MIDI trigger: note, control, program.
//
//////////////////////////////////////////////////////////////////////

class MidiTypeVariableType : public ScriptInternalVariable {
  public:
    virtual ~MidiTypeVariableType() {}
    MidiTypeVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

MidiTypeVariableType::MidiTypeVariableType()
{
    setName("midiType");
}

void MidiTypeVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	const char* type = "unknown";

    int id = si->getTriggerId();
	int status = (id >> 12);

	switch (status) {
		case 0x9: type = "note"; break;
		case 0xB: type = "control"; break;
		case 0xC: type = "program"; break;
		case 0xD: type = "touch"; break;
		case 0xE: type = "bend"; break;
	}

	value->setString(type);
}

MidiTypeVariableType MidiTypeVariableObj;
ScriptInternalVariable* MidiTypeVariable = &MidiTypeVariableObj;

//////////////////////////////////////////////////////////////////////
//
// midiChannel
//
// The MIDI channel number of the trigger event.
// This is also embedded in triggerNumber, but 
//
//////////////////////////////////////////////////////////////////////

class MidiChannelVariableType : public ScriptInternalVariable {
  public:
    virtual ~MidiChannelVariableType() {}
    MidiChannelVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

MidiChannelVariableType::MidiChannelVariableType()
{
    setName("midiChannel");
}

void MidiChannelVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	int id = si->getTriggerId();
	int channel = (id >> 8) & 0xF;
	value->setInt(channel);
}

MidiChannelVariableType MidiChannelVariableObj;
ScriptInternalVariable* MidiChannelVariable = &MidiChannelVariableObj;

//////////////////////////////////////////////////////////////////////
//
// midiNumber
//
// The MIDI key/controller number of the trigger event.
//
//////////////////////////////////////////////////////////////////////

class MidiNumberVariableType : public ScriptInternalVariable {
  public:
    virtual ~MidiNumberVariableType() {}
    MidiNumberVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

MidiNumberVariableType::MidiNumberVariableType()
{
    setName("midiNumber");
}

void MidiNumberVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	int id = si->getTriggerId();
	int number = (id & 0xFF);
	value->setInt(number);
}

MidiNumberVariableType MidiNumberVariableObj;
ScriptInternalVariable* MidiNumberVariable = &MidiNumberVariableObj;

//////////////////////////////////////////////////////////////////////
//
// midiValue
//
// The same as triggerValue but has a more obvious name for
// use in !controller scripts.
//
//////////////////////////////////////////////////////////////////////

class MidiValueVariableType : public ScriptInternalVariable {
  public:
    virtual ~MidiValueVariableType() {}
    MidiValueVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

MidiValueVariableType::MidiValueVariableType()
{
    setName("midiValue");
}

void MidiValueVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getTriggerValue());
}

MidiValueVariableType MidiValueVariableObj;
ScriptInternalVariable* MidiValueVariable = &MidiValueVariableObj;

//////////////////////////////////////////////////////////////////////
//
// returnCode
//
// The return code of the last KernelEvent.
// Currently used only by Prompt statements to convey the 
// selected button.  0 means Ok, 1 means cancel.
//
//////////////////////////////////////////////////////////////////////

class ReturnCodeVariableType : public ScriptInternalVariable {
  public:
    virtual ~ReturnCodeVariableType() {}
    ReturnCodeVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
	void setValue(ScriptInterpreter* si, ExValue* value);
};

ReturnCodeVariableType::ReturnCodeVariableType()
{
    setName("returnCode");
}

void ReturnCodeVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getReturnCode());
}

void ReturnCodeVariableType::setValue(ScriptInterpreter* si, 
											 ExValue* value)
{
	si->setReturnCode(value->getInt());
}

ReturnCodeVariableType ReturnCodeVariableObj;
ScriptInternalVariable* ReturnCodeVariable = &ReturnCodeVariableObj;

/****************************************************************************
 *                                                                          *
 *   							INTERNAL STATE                              *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// blockFrames
//
// The number of frames in one audio interrupt block.
//
//////////////////////////////////////////////////////////////////////

class BlockFramesVariableType : public ScriptInternalVariable {
  public:
    virtual ~BlockFramesVariableType() {}
    BlockFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

BlockFramesVariableType::BlockFramesVariableType()
{
    setName("blockFrames");
}

void BlockFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
    (void)t;
    // !! need to be checking the MobiusContainer
	value->setLong(AUDIO_FRAMES_PER_BUFFER);
}

BlockFramesVariableType BlockFramesVariableObj;
ScriptInternalVariable* BlockFramesVariable = &BlockFramesVariableObj;

//////////////////////////////////////////////////////////////////////
//
// sampleFrames
//
// The number of frames in the last sample we played.
//
//////////////////////////////////////////////////////////////////////

class SampleFramesVariableType : public ScriptInternalVariable {
  public:
    virtual ~SampleFramesVariableType() {}
    SampleFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SampleFramesVariableType::SampleFramesVariableType()
{
    setName("sampleFrames");
}

void SampleFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
    // this used to live on Mobius but it has been moved up to Kernel
    // used only by test scripts that want to wait for the last trigger
    // sample to finish playing
    long frames = t->getMobius()->getKernel()->getLastSampleFrames();
    value->setLong(frames);
}

SampleFramesVariableType SampleFramesVariableObj;
ScriptInternalVariable* SampleFramesVariable = &SampleFramesVariableObj;

/****************************************************************************
 *                                                                          *
 *   						  CONTROL VARIABLES                             *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// noExternalAudio
//
// When set disables the pass through of audio received
// on the first port.  This is used in the unit tests that do their
// own audio injection, and we don't want random noise comming
// in from the sound card to pollute it.
//
//////////////////////////////////////////////////////////////////////

class NoExternalAudioVariableType : public ScriptInternalVariable {
  public:
    virtual ~NoExternalAudioVariableType() {}
    NoExternalAudioVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
	void setValue(ScriptInterpreter* si, ExValue* value);
};

NoExternalAudioVariableType::NoExternalAudioVariableType()
{
    setName("noExternalAudio");
}

void NoExternalAudioVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	Mobius* m = si->getMobius();
    MobiusKernel* k = m->getKernel();
	value->setBool(k->isNoExternalInput());
}

void NoExternalAudioVariableType::setValue(ScriptInterpreter* si, 
										   ExValue* value)
{
	Mobius* m = si->getMobius();
    MobiusKernel* k = m->getKernel();
	k->setNoExternalInput(value->getBool());
}

NoExternalAudioVariableType NoExternalAudioVariableObj;
ScriptInternalVariable* NoExternalAudioVariable = &NoExternalAudioVariableObj;

/****************************************************************************
 *                                                                          *
 *   							  LOOP STATE                                *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// loopCount
//
// The current loop count.
// This is effectively the same as the "moreLoops" parameter but
// I like this name better.  This should really be an alias of moreLoops
// so we can get and set it using the same name!!
//
//////////////////////////////////////////////////////////////////////

class LoopCountVariableType : public ScriptInternalVariable {

  public:
    virtual ~LoopCountVariableType() {}
    LoopCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

LoopCountVariableType::LoopCountVariableType()
{
    setName("loopCount");
}

void LoopCountVariableType::getTrackValue(Track* t, ExValue* value)
{
    value->setInt(t->getLoopCount());
}

LoopCountVariableType LoopCountVariableObj;
ScriptInternalVariable* LoopCountVariable = &LoopCountVariableObj;

//////////////////////////////////////////////////////////////////////
//
// loopNumber
//
// The number of the curerent loop within the track.  The first
// loop number is one for consistency with the trigger functions
// Loop1, Loop2, etc.
//
// This is similar to "mode" in that it conveys read-only information
// about the current loop.  Mode is however implemented as a Parameter.
// Now that we have internal script variables, mode would make more sense
// over here, but being a Parameter gives it some localization support
// so leave it there for now.
//
//////////////////////////////////////////////////////////////////////

class LoopNumberVariableType : public ScriptInternalVariable {
  public:
    virtual ~LoopNumberVariableType() {}
    LoopNumberVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

LoopNumberVariableType::LoopNumberVariableType()
{
    setName("loopNumber");
}

void LoopNumberVariableType::getTrackValue(Track* t, ExValue* value)
{
	// note that internally loops are numbered from 1
    value->setInt(t->getLoop()->getNumber());
}

LoopNumberVariableType LoopNumberVariableObj;
ScriptInternalVariable* LoopNumberVariable = &LoopNumberVariableObj;

//////////////////////////////////////////////////////////////////////
//
// loopFrames
//
// The number of frames in the loop.
//
//////////////////////////////////////////////////////////////////////

class LoopFramesVariableType : public ScriptInternalVariable {
  public:
    virtual ~LoopFramesVariableType() {}
    LoopFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

LoopFramesVariableType::LoopFramesVariableType()
{
    setName("loopFrames");
}

void LoopFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getFrames());
}

LoopFramesVariableType LoopFramesVariableObj;
ScriptInternalVariable* LoopFramesVariable = &LoopFramesVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// loopFrame
// 
// The current record frame.
//
//////////////////////////////////////////////////////////////////////

class LoopFrameVariableType : public ScriptInternalVariable {
  public:
    virtual ~LoopFrameVariableType() {}
    LoopFrameVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

LoopFrameVariableType::LoopFrameVariableType()
{
    setName("loopFrame");
}

void LoopFrameVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getFrame());
}

LoopFrameVariableType LoopFrameVariableObj;
ScriptInternalVariable* LoopFrameVariable = &LoopFrameVariableObj;

//////////////////////////////////////////////////////////////////////
//
// cycleCount
//
// The number of cycles in the loop.
//
//////////////////////////////////////////////////////////////////////

class CycleCountVariableType : public ScriptInternalVariable {
  public:
    virtual ~CycleCountVariableType() {}
    CycleCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
    void setValue(ScriptInterpreter* si, ExValue* value);
};

CycleCountVariableType::CycleCountVariableType()
{
    setName("cycleCount");
}

void CycleCountVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getCycles());
}

/**
 * This is one of the few variables that has a setter.
 * 
 * Changing the cycle size can have all sorts of subtle consequences
 * for synchronization so you should only do this if sync is off or
 * we've already locked the trackers.  That's hard to prevent here, 
 * could potentially schedule a pending Event to change the cycle count
 * when the sync dust settles.  
 *
 * Trying to dynamically adjust Synchronizer and SyncTracker for cycle
 * length changes is even harder.  We don't call 
 * Synchronizer::resizeOutSyncTracker since the size hasn't actually 
 * changed and the new cycleFrames could cause those calculations to
 * pick a different tempo.
 *
 * This will not change quantization of previously scheduled events.
 * 
 * This will change the record layer cycle count but not the play layer.
 * It currently does not shift a layer so this is not an undoable action.
 * If you undo the cycle count will revert to what it is in the play layer.
 *
 */
void CycleCountVariableType::setValue(ScriptInterpreter* si, 
                                             ExValue* value)
{
    Track* t = si->getTargetTrack();
    Loop* l = t->getLoop();
    l->setCycles(value->getInt());
}

CycleCountVariableType CycleCountVariableObj;
ScriptInternalVariable* CycleCountVariable = &CycleCountVariableObj;
 
//////////////////////////////////////////////////////////////////////
//
// cycleNumber
//
// The current cycle number, relative to the beginning of the loop.
//
//////////////////////////////////////////////////////////////////////

class CycleNumberVariableType : public ScriptInternalVariable {
  public:
    virtual ~CycleNumberVariableType() {}
    CycleNumberVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

CycleNumberVariableType::CycleNumberVariableType()
{
    setName("cycleNumber");
}

void CycleNumberVariableType::getTrackValue(Track* t, ExValue* value)
{
    Loop* l = t->getLoop();
    long frame = l->getFrame();
    long cycleFrames = l->getCycleFrames();
    value->setLong(frame / cycleFrames);
}

CycleNumberVariableType CycleNumberVariableObj;
ScriptInternalVariable* CycleNumberVariable = &CycleNumberVariableObj;

//////////////////////////////////////////////////////////////////////
//
// cycleFrames
//
// The number of frames in one cycle.
// 
//////////////////////////////////////////////////////////////////////

class CycleFramesVariableType : public ScriptInternalVariable {
  public:
    virtual ~CycleFramesVariableType() {}
    CycleFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

CycleFramesVariableType::CycleFramesVariableType()
{
    setName("cycleFrames");
}

void CycleFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
    value->setLong(t->getLoop()->getCycleFrames());
}

CycleFramesVariableType CycleFramesVariableObj;
ScriptInternalVariable* CycleFramesVariable = &CycleFramesVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// cycleFrame
//
// The current frame relative the current cycle.
//
//////////////////////////////////////////////////////////////////////

class CycleFrameVariableType : public ScriptInternalVariable {
  public:
    virtual ~CycleFrameVariableType() {}
    CycleFrameVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

CycleFrameVariableType::CycleFrameVariableType()
{
    setName("cycleFrame");
}

void CycleFrameVariableType::getTrackValue(Track* t, ExValue* value)
{
    Loop* l = t->getLoop();
    long frame = l->getFrame();
    long cycleFrames = l->getCycleFrames();
	value->setLong(frame % cycleFrames);
}

CycleFrameVariableType CycleFrameVariableObj;
ScriptInternalVariable* CycleFrameVariable = &CycleFrameVariableObj;

//////////////////////////////////////////////////////////////////////
//
// subCycleCount
// 
// The number of subCycles in a cycle.
// This is actually the same as the "subcycles" preset parameter and
// can change with the preset, but we expose it as an internal variable
// so it is consistent with the other loop divisions.
//
//////////////////////////////////////////////////////////////////////

class SubCycleCountVariableType : public ScriptInternalVariable {
  public:
    virtual ~SubCycleCountVariableType() {}
    SubCycleCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SubCycleCountVariableType::SubCycleCountVariableType()
{
    setName("subCycleCount");
}

void SubCycleCountVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(ParameterSource::getSubcycles(t));
}

SubCycleCountVariableType SubCycleCountVariableObj;
ScriptInternalVariable* SubCycleCountVariable = &SubCycleCountVariableObj;

//////////////////////////////////////////////////////////////////////
//
// subCycleNumber
// 
// The current subcycle number, relative to the current cycle.
// !! Should this be relative to the start of the loop?
//
//////////////////////////////////////////////////////////////////////

class SubCycleNumberVariableType : public ScriptInternalVariable {
  public:
    virtual ~SubCycleNumberVariableType() {}
    SubCycleNumberVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SubCycleNumberVariableType::SubCycleNumberVariableType()
{
    setName("subCycleNumber");
}

void SubCycleNumberVariableType::getTrackValue(Track* t, ExValue* value)
{
    Loop* l = t->getLoop();
    long frame = l->getFrame();
    long subCycleFrames = l->getSubCycleFrames();

    // absolute subCycle with in loop
    long subCycle = frame / subCycleFrames;

    // adjust to be relative to start of cycle
    subCycle %= ParameterSource::getSubcycles(t);

	value->setLong(subCycle);
}

SubCycleNumberVariableType SubCycleNumberVariableObj;
ScriptInternalVariable* SubCycleNumberVariable = &SubCycleNumberVariableObj;

//////////////////////////////////////////////////////////////////////
//
// subCycleFrames
// 
// The number of frames in one subcycle.
//
//////////////////////////////////////////////////////////////////////

class SubCycleFramesVariableType : public ScriptInternalVariable {
  public:
    virtual ~SubCycleFramesVariableType() {}
    SubCycleFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SubCycleFramesVariableType::SubCycleFramesVariableType()
{
    setName("subCycleFrames");
}

void SubCycleFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getSubCycleFrames());
}

SubCycleFramesVariableType SubCycleFramesVariableObj;
ScriptInternalVariable* SubCycleFramesVariable = &SubCycleFramesVariableObj;

//////////////////////////////////////////////////////////////////////
//
// subCycleFrame
//
// The current frame relative the current subcycle.
//
//////////////////////////////////////////////////////////////////////

class SubCycleFrameVariableType : public ScriptInternalVariable {
  public:
    virtual ~SubCycleFrameVariableType() {}
    SubCycleFrameVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SubCycleFrameVariableType::SubCycleFrameVariableType()
{
    setName("subCycleFrame");
}

void SubCycleFrameVariableType::getTrackValue(Track* t, ExValue* value)
{
    Loop* l = t->getLoop();
    long frame = l->getFrame();
    long subCycleFrames = l->getSubCycleFrames();

	value->setLong(frame % subCycleFrames);
}

SubCycleFrameVariableType SubCycleFrameVariableObj;
ScriptInternalVariable* SubCycleFrameVariable = &SubCycleFrameVariableObj;

//////////////////////////////////////////////////////////////////////
//
// layerCount
// 
// The number of layers in the current loop.  This is also
// in effect the current layer number since we are always "on"
// the last layer of the loop.  This does not include the number
// of available redo layers.
// 
//////////////////////////////////////////////////////////////////////

class LayerCountVariableType : public ScriptInternalVariable {
  public:
    virtual ~LayerCountVariableType() {}
    LayerCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

LayerCountVariableType::LayerCountVariableType()
{
    setName("layerCount");
}

void LayerCountVariableType::getTrackValue(Track* t, ExValue* value)
{
	Loop* loop = t->getLoop();
	int count = 0;

	// count backwards from the play layer, the record layer is invisible
	// ?? might want a variable to display the number of *visible*
	// layers if checkpoints are being used

	for (Layer* l = loop->getPlayLayer() ; l != nullptr ; l = l->getPrev())
	  count++;

	value->setInt(count);
}

LayerCountVariableType LayerCountVariableObj;
ScriptInternalVariable* LayerCountVariable = &LayerCountVariableObj;

//////////////////////////////////////////////////////////////////////
//
// redoCount
// 
// The number of redo layers in the current loop.  
// 
//////////////////////////////////////////////////////////////////////

class RedoCountVariableType : public ScriptInternalVariable {
  public:
    virtual ~RedoCountVariableType() {}
    RedoCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

RedoCountVariableType::RedoCountVariableType()
{
    setName("redoCount");
}

void RedoCountVariableType::getTrackValue(Track* t, ExValue* value)
{
	Loop* loop = t->getLoop();
	int count = 0;

	// The redo list uses the mRedo field with each link 
	// being a possible checkpoint chain using the mPrev field.
	// Since layerCount returns all layers, not just the visible ones
	// we probably need to do the same here.

	for (Layer* redo = loop->getRedoLayer(); redo != nullptr ; 
		 redo = redo->getRedo()) {

		for (Layer* l = redo ; l != nullptr ; l = l->getPrev())
		  count++;
	}

	value->setInt(count);
}

RedoCountVariableType RedoCountVariableObj;
ScriptInternalVariable* RedoCountVariable = &RedoCountVariableObj;

//////////////////////////////////////////////////////////////////////
//
// effectiveFeedback
// 
// The value of the feedback currently being applied.  This
// will either be the FeedbackLevel or AltFeedbackLevel parameter values
// depending on AltFeedbackEnable.  It will be zero if we're in Replace, Insert
// or another mode that does not bring forward any content from the
// previous loop.
// 
//////////////////////////////////////////////////////////////////////

class EffectiveFeedbackVariableType : public ScriptInternalVariable {
  public:
    virtual ~EffectiveFeedbackVariableType() {}
    EffectiveFeedbackVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

EffectiveFeedbackVariableType::EffectiveFeedbackVariableType()
{
    setName("effectiveFeedback");
}

void EffectiveFeedbackVariableType::getTrackValue(Track* t, ExValue* value)
{
	Loop* loop = t->getLoop();
    value->setInt(loop->getEffectiveFeedback());
}

EffectiveFeedbackVariableType EffectiveFeedbackVariableObj;
ScriptInternalVariable* EffectiveFeedbackVariable = &EffectiveFeedbackVariableObj;

/****************************************************************************
 *                                                                          *
 *   								EVENTS                                  *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// nextEvent
//
// Returns the type name of the next event.  Child events are ignored
// so we will skip over JumpPlayEvents.  
// Now that we have this, could eliminate InReturn and InRealign.
//
//////////////////////////////////////////////////////////////////////

class NextEventVariableType : public ScriptInternalVariable {
  public:
    virtual ~NextEventVariableType() {}
    NextEventVariableType();
    void getTrackValue(Track* t, ExValue* value);
    virtual const char* getReturnValue(Event* e);
};

NextEventVariableType::NextEventVariableType()
{
    setName("nextEvent");
}

void NextEventVariableType::getTrackValue(Track* t, ExValue* value)
{
	Event* found = nullptr;
    EventManager* em = t->getEventManager();

	// Return the next parent event.  Assuming that these will
	// be scheduled in time order so we don't have to sort them.
	// Since we're "in the interrupt" and not modifying the list, we
	// don't have to worry about csects

	for (Event* e = em->getEvents() ; e != nullptr ; e = e->getNext()) {
		if (e->getParent() == nullptr) {
			found = e;
			break;
		}
	}

	if (found == nullptr)
	  value->setNull();
	else 
	  value->setString(getReturnValue(found));
}

const char* NextEventVariableType::getReturnValue(Event* e)
{
	return e->type->name;
}

NextEventVariableType NextEventVariableObj;
ScriptInternalVariable* NextEventVariable = &NextEventVariableObj;

//
// nextEventFunction
//
// Returns the function name associated with the next event.
// We subclass NextEventVariableType for the getTrackValue logic.
//
//////////////////////////////////////////////////////////////////////

class NextEventFunctionVariableType : public NextEventVariableType {
  public:
    virtual ~NextEventFunctionVariableType() {}
    NextEventFunctionVariableType();
    const char* getReturnValue(Event* e);
};

NextEventFunctionVariableType::NextEventFunctionVariableType()
{
    setName("nextEventFunction");
}

/**
 * Overloaded from NextEventVariableType to return the 
 * associated function name rather than the event type name.
 */
const char* NextEventFunctionVariableType::getReturnValue(Event* e)
{
	const char* value = nullptr;
	Function* f = e->function;
	if (f != nullptr)
	  value = f->getName();
	return value;
}

NextEventFunctionVariableType NextEventFunctionVariableObj;
ScriptInternalVariable* NextEventFunctionVariable = &NextEventFunctionVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// nextLoop
//
// The number of the next loop if we're in loop switch mode.
// Loops are numbered from 1.  Returns zero if we're not loop switching.
// 
// !! This is something that would be useful to change.
// 
//////////////////////////////////////////////////////////////////////

class NextLoopVariableType : public ScriptInternalVariable {
  public:
    virtual ~NextLoopVariableType() {}
    NextLoopVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

NextLoopVariableType::NextLoopVariableType()
{
    setName("nextLoop");
}

void NextLoopVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getLoop()->getNextLoop());
}

NextLoopVariableType NextLoopVariableObj;
ScriptInternalVariable* NextLoopVariable = &NextLoopVariableObj;

//
// eventSummary
//
// Returns a string representation of all scheduled events.
// This is intended only for testing, the syntax is undefined.
//
//////////////////////////////////////////////////////////////////////

class EventSummaryVariableType : public ScriptInternalVariable {
  public:
    virtual ~EventSummaryVariableType() {}
    EventSummaryVariableType();
    void getTrackValue(Track* t, ExValue* value);
  private:
    int getEventIndex(Event* list, Event* event);

};

EventSummaryVariableType::EventSummaryVariableType()
{
    setName("eventSummary");
}

void EventSummaryVariableType::getTrackValue(Track* t, ExValue* value)
{
    EventManager* em = t->getEventManager();

    // in theory this can be large, so use a Vbuf
    Vbuf* buf = NEW(Vbuf);

    Event* eventList = em->getEvents();
    int ecount = 0;
	for (Event* e = eventList ; e != nullptr ; e = e->getNext()) {

        if (ecount > 0)
          buf->add(",");
        ecount++;

        buf->add(e->type->name);
        buf->add("(");
        if (e->pending)
          buf->add("pending");
        else {
            buf->add("f=");
            buf->add((int)(e->frame));
        }

        if (e->getChildren() != nullptr) {
            int ccount = 0;
            buf->add(",c=");
            for (Event* c = e->getChildren() ; c != nullptr ; c = c->getSibling()) {
                if (ccount > 0)
                  buf->add(",");
                ccount++;
                // prefix scheduled events with a number so we can
                // see sharing
                if (c->getList() != nullptr) {
                    buf->add(getEventIndex(eventList, c));
                    buf->add(":");
                }
                buf->add(c->type->name);
            }
        }

        buf->add(")");
	}
    
    if (buf->getSize() == 0)
      value->setNull();
    else
      value->setString(buf->getBuffer());

    delete buf;
}

int EventSummaryVariableType::getEventIndex(Event* list, Event* event)
{
    int index = 0;

    if (list != nullptr && event != nullptr) {
        int i = 1;
        for (Event* e = list ; e != nullptr ; e = e->getNext()) {
            if (e != event)
              i++;
            else {
                index = i;
                break;
            }
        }
    }

    return index;
}


EventSummaryVariableType EventSummaryVariableObj;
ScriptInternalVariable* EventSummaryVariable = &EventSummaryVariableObj;

//////////////////////////////////////////////////////////////////////
//
// Modes
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// 
// mode
//
// Name of the current mode.
//
//////////////////////////////////////////////////////////////////////

class ModeVariableType : public ScriptInternalVariable {
  public:
    virtual ~ModeVariableType() {}
    ModeVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

ModeVariableType::ModeVariableType()
{
    setName("mode");
}

void ModeVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setString(t->getLoop()->getMode()->getName());
}

ModeVariableType ModeVariableObj;
ScriptInternalVariable* ModeVariable = &ModeVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// isRecording
//
// True any form of recording is being performed.  Note that this
// does not necessarily mean you are in Record mode, you could be in 
// Overdub, Multiply, Insert, etc.
//
//////////////////////////////////////////////////////////////////////

class IsRecordingVariableType : public ScriptInternalVariable {
  public:
    virtual ~IsRecordingVariableType() {}
    IsRecordingVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

IsRecordingVariableType::IsRecordingVariableType()
{
    setName("isRecording");
}

void IsRecordingVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getLoop()->isRecording());
}

IsRecordingVariableType IsRecordingVariableObj;
ScriptInternalVariable* IsRecordingVariable = &IsRecordingVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// inOverdub
//
// True if overdub is enabled.   Note that this doesn't necessarily
// mean that the mode is overdub, only that overdub is enabled when
// we fall back into Play mode.
//
//////////////////////////////////////////////////////////////////////

class InOverdubVariableType : public ScriptInternalVariable {
  public:
    virtual ~InOverdubVariableType() {}
    InOverdubVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InOverdubVariableType::InOverdubVariableType()
{
    setName("inOverdub");
}

void InOverdubVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getLoop()->isOverdub());
}

InOverdubVariableType InOverdubVariableObj;
ScriptInternalVariable* InOverdubVariable = &InOverdubVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// inHalfspeed
//
// True if half-speed is enabled.
//
//////////////////////////////////////////////////////////////////////

class InHalfspeedVariableType : public ScriptInternalVariable {
  public:
    virtual ~InHalfspeedVariableType() {}
    InHalfspeedVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InHalfspeedVariableType::InHalfspeedVariableType()
{
    setName("inHalfspeed");
}

/**
 * This is more complicated now that we've genearlized speed shift.
 * Assume that if the rate toggle is -12 we're in half speed.
 */
void InHalfspeedVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getSpeedToggle() == -12);
}

InHalfspeedVariableType InHalfspeedVariableObj;
ScriptInternalVariable* InHalfspeedVariable = &InHalfspeedVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// inReverse
//
// True if reverse is enabled.
// Would be nice to have "direction" with values "reverse" and "forward"?
//
//////////////////////////////////////////////////////////////////////

class InReverseVariableType : public ScriptInternalVariable {
  public:
    virtual ~InReverseVariableType() {}
    InReverseVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InReverseVariableType::InReverseVariableType()
{
    setName("inReverse");
}

void InReverseVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getLoop()->isReverse());
}

InReverseVariableType InReverseVariableObj;
ScriptInternalVariable* InReverseVariable = &InReverseVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// inMute
//
// True if playback is muted.  This usually means that we're
// also in Mute mode, but if Overdub is also on, mode
// will be Overdub.  Note also that this tests the isMute flag
// which can be on for other reasons than being in Mute mode.
// 
// !! Think more about who should win.
// Do we need both inMuteMode and inMute or maybe
// isMuted and inMuteMode
//
//////////////////////////////////////////////////////////////////////

class InMuteVariableType : public ScriptInternalVariable {
  public:
    virtual ~InMuteVariableType() {}
    InMuteVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InMuteVariableType::InMuteVariableType()
{
    setName("inMute");
}

void InMuteVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getLoop()->isMuteMode());
}

InMuteVariableType InMuteVariableObj;
ScriptInternalVariable* InMuteVariable = &InMuteVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// inPause
//
// True if we're in Pause or Pause mode.
// This is available because the "mode" parameter is not always
// set to Pause.  Once case is if Pause and Overdub are on at the same
// time mode will be Overdub (I think this is the only case).
//
//////////////////////////////////////////////////////////////////////

class InPauseVariableType : public ScriptInternalVariable {
  public:
    virtual ~InPauseVariableType() {}
    InPauseVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InPauseVariableType::InPauseVariableType()
{
    setName("inPause");
}

void InPauseVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getLoop()->isPaused());
}

InPauseVariableType InPauseVariableObj;
ScriptInternalVariable* InPauseVariable = &InPauseVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// inRealign
//
// True if we're realigning.  This similar to a mode, but 
// it is indiciated by having a Realign event scheduled.
//
//////////////////////////////////////////////////////////////////////

class InRealignVariableType : public ScriptInternalVariable {
  public:
    virtual ~InRealignVariableType() {}
    InRealignVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InRealignVariableType::InRealignVariableType()
{
    setName("inRealign");
}

void InRealignVariableType::getTrackValue(Track* t, ExValue* value)
{
    EventManager* em = t->getEventManager();
	Event* e = em->findEvent(RealignEvent);
	value->setBool(e != nullptr);
}

InRealignVariableType InRealignVariableObj;
ScriptInternalVariable* InRealignVariable = &InRealignVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// inReturn
//
// True if we're in "return" mode.  This is a special minor mode that
// happens after a loop switch with SwitchDuration=OnceReturn, 
// SwitchDuration=SustainReturn, or the RestartOnce function.  
// It is indiciated by the presence of a pending Return event.
//
//////////////////////////////////////////////////////////////////////

class InReturnVariableType : public ScriptInternalVariable {
  public:
    virtual ~InReturnVariableType() {}
    InReturnVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InReturnVariableType::InReturnVariableType()
{
    setName("inReturn");
}

void InReturnVariableType::getTrackValue(Track* t, ExValue* value)
{
    EventManager* em = t->getEventManager();
	Event* e = em->findEvent(ReturnEvent);
	value->setBool(e != nullptr);
}

InReturnVariableType InReturnVariableObj;
ScriptInternalVariable* InReturnVariable = &InReturnVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// rate
//
// Same as the speedStep parameter.  I would rather not have this but it's
// been used for a long time.
//
// We also exposed "scaleRate" for awhile but I don't think that
// was in wide use.  Get everyone to move over to speedStep.
//
//////////////////////////////////////////////////////////////////////

class RateVariableType : public ScriptInternalVariable {
  public:
    virtual ~RateVariableType() {}
    RateVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

RateVariableType::RateVariableType()
{
    setName("rate");
}

void RateVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getSpeedStep());
}

RateVariableType RateVariableObj;
ScriptInternalVariable* RateVariable = &RateVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// rawSpeed, rawRate
//
// Playback speed, expressed as a float x1000000.
// rawRate was the original name but we can probably get rid of that
// as it's only useful in scripts.
//
// !! effectiveSpeed would be better
//
//////////////////////////////////////////////////////////////////////

class RawSpeedVariableType : public ScriptInternalVariable {
  public:
    virtual ~RawSpeedVariableType() {}
    RawSpeedVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

RawSpeedVariableType::RawSpeedVariableType()
{
    setName("rawSpeed");
    setAlias("rawRate");
}

void RawSpeedVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong((long)(t->getEffectiveSpeed() * 1000000));
}

RawSpeedVariableType RawSpeedVariableObj;
ScriptInternalVariable* RawSpeedVariable = &RawSpeedVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// rawPitch
//
// Playback pitch, expressed as a float x1000000.
//
//////////////////////////////////////////////////////////////////////

class RawPitchVariableType : public ScriptInternalVariable {
  public:
    virtual ~RawPitchVariableType() {}
    RawPitchVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

RawPitchVariableType::RawPitchVariableType()
{
    setName("rawPitch");
}

void RawPitchVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong((long)(t->getEffectivePitch() * 1000000));
}

RawPitchVariableType RawPitchVariableObj;
ScriptInternalVariable* RawPitchVariable = &RawPitchVariableObj;

//////////////////////////////////////////////////////////////////////
//
// speedToggle
//
// The effective speed toggle in a track.
// This is a generalization of Halfspeed, the SpeedToggle script function
// can be used to toggle on or off at any step interval.
//
//////////////////////////////////////////////////////////////////////

class SpeedToggleVariableType : public ScriptInternalVariable {
  public:
    virtual ~SpeedToggleVariableType() {}
    SpeedToggleVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SpeedToggleVariableType::SpeedToggleVariableType()
{
    setName("speedToggle");
}

void SpeedToggleVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getSpeedToggle());
}

SpeedToggleVariableType SpeedToggleVariableObj;
ScriptInternalVariable* SpeedToggleVariable = &SpeedToggleVariableObj;

//////////////////////////////////////////////////////////////////////
//
// speedSequenceIndex
//
// The speed sequence index in a track.
// This is normally changed by the SpeedNext and SpeedPrev functions,
// and may be set to a specific value through this variable.
//
//////////////////////////////////////////////////////////////////////

class SpeedSequenceIndexVariableType : public ScriptInternalVariable {
  public:
    virtual ~SpeedSequenceIndexVariableType() {}
    SpeedSequenceIndexVariableType();
    void getTrackValue(Track* t, ExValue* value);
    void setValue(ScriptInterpreter* si, ExValue* value);
};

SpeedSequenceIndexVariableType::SpeedSequenceIndexVariableType()
{
    setName("speedSequenceIndex");
}

void SpeedSequenceIndexVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getSpeedSequenceIndex());
}

void SpeedSequenceIndexVariableType::setValue(ScriptInterpreter* si, 
                                              ExValue* value)
{
    int index = value->getInt();
    // Track doesn't do any range checking, at least
    // catch negatives, could check the sequence parameter
    if (index < 0) index = 0;

    Track* t = si->getTargetTrack();
    t->setSpeedSequenceIndex(index);
}

SpeedSequenceIndexVariableType SpeedSequenceIndexVariableObj;
ScriptInternalVariable* SpeedSequenceIndexVariable = &SpeedSequenceIndexVariableObj;

//////////////////////////////////////////////////////////////////////
//
// pitchSequenceIndex
//
// The pitch sequence index in a track.
// This is normally changed by the PitchNext and PitchPrev functions,
// and may be set to a specific value through this variable.
//
//////////////////////////////////////////////////////////////////////

class PitchSequenceIndexVariableType : public ScriptInternalVariable {
  public:
    virtual ~PitchSequenceIndexVariableType() {}
    PitchSequenceIndexVariableType();
    void getTrackValue(Track* t, ExValue* value);
    void setValue(ScriptInterpreter* si, ExValue* value);
};

PitchSequenceIndexVariableType::PitchSequenceIndexVariableType()
{
    setName("pitchSequenceIndex");
}

void PitchSequenceIndexVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getPitchSequenceIndex());
}

void PitchSequenceIndexVariableType::setValue(ScriptInterpreter* si, 
                                              ExValue* value)
{
    int index = value->getInt();
    // Track doesn't do any range checking, at least
    // catch negatives, could check the sequence parameter
    if (index < 0) index = 0;

    Track* t = si->getTargetTrack();
    t->setPitchSequenceIndex(index);
}

PitchSequenceIndexVariableType PitchSequenceIndexVariableObj;
ScriptInternalVariable* PitchSequenceIndexVariable = &PitchSequenceIndexVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// historyFrames
//
// The total number of frames in all loop layers.
// Used to determine the relative location of the loop window.
//
//////////////////////////////////////////////////////////////////////

class HistoryFramesVariableType : public ScriptInternalVariable {
  public:
    virtual ~HistoryFramesVariableType() {}
    HistoryFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

HistoryFramesVariableType::HistoryFramesVariableType()
{
    setName("historyFrames");
}

void HistoryFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getHistoryFrames());
}

HistoryFramesVariableType HistoryFramesVariableObj;
ScriptInternalVariable* HistoryFramesVariable = &HistoryFramesVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// windowOffset
//
// The offset in frames of the current loop window within the
// entire loop history.  If a window is not active the value is -1.
//
//////////////////////////////////////////////////////////////////////

class WindowOffsetVariableType : public ScriptInternalVariable {
  public:
    virtual ~WindowOffsetVariableType() {}
    WindowOffsetVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

WindowOffsetVariableType::WindowOffsetVariableType()
{
    setName("windowOffset");
}

void WindowOffsetVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getWindowOffset());
}

WindowOffsetVariableType WindowOffsetVariableObj;
ScriptInternalVariable* WindowOffsetVariable = &WindowOffsetVariableObj;

/****************************************************************************
 *                                                                          *
 *   							 TRACK STATE                                *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// trackCount
//
// The number of tracks configured.
//
//////////////////////////////////////////////////////////////////////

class TrackCountVariableType : public ScriptInternalVariable {

  public:
    virtual ~TrackCountVariableType() {}
    TrackCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

TrackCountVariableType::TrackCountVariableType()
{
    setName("trackCount");
}

void TrackCountVariableType::getTrackValue(Track* t, ExValue* value)
{
	Mobius* m = t->getMobius();
    value->setInt(m->getTrackCount());
}

TrackCountVariableType TrackCountVariableObj;
ScriptInternalVariable* TrackCountVariable = &TrackCountVariableObj;


//////////////////////////////////////////////////////////////////////
//
// track, trackNumber
// 
// The number of the current track.  The first track is 1.
// 
//////////////////////////////////////////////////////////////////////

class TrackVariableType : public ScriptInternalVariable {
  public:
    virtual ~TrackVariableType() {}
    TrackVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

TrackVariableType::TrackVariableType()
{
    setName("track");
	// for consistency with loopNumber and layerNumber
    setAlias("trackNumber");
}

void TrackVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getDisplayNumber());
}

TrackVariableType TrackVariableObj;
ScriptInternalVariable* TrackVariable = &TrackVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// globalMute
// 
// True if the track will be unmuted when Global Mute mode is over.
//
// ?? Do we need a simple global variable to indiciate that we're in
// global mute mode?
//
//////////////////////////////////////////////////////////////////////

class GlobalMuteVariableType : public ScriptInternalVariable {
  public:
    virtual ~GlobalMuteVariableType() {}
    GlobalMuteVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

GlobalMuteVariableType::GlobalMuteVariableType()
{
    setName("globalMute");
}

void GlobalMuteVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->isGlobalMute());
}

GlobalMuteVariableType GlobalMuteVariableObj;
ScriptInternalVariable* GlobalMuteVariable = &GlobalMuteVariableObj;

//////////////////////////////////////////////////////////////////////
// 
// solo
// 
//
// True if the track will be unmuted when Global Mute mode is over.
//
// ?? Do we need a simple global variable to indiciate that we're in
// global mute mode?
//
//////////////////////////////////////////////////////////////////////

class SoloVariableType : public ScriptInternalVariable {
  public:
    virtual ~SoloVariableType() {}
    SoloVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SoloVariableType::SoloVariableType()
{
    setName("solo");
}

void SoloVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->isSolo());
}

SoloVariableType SoloVariableObj;
ScriptInternalVariable* SoloVariable = &SoloVariableObj;

//////////////////////////////////////////////////////////////////////
//
// trackSyncMaster
//
// The number of the track operating as the track sync master,
// 0 if there is no master.
// 
// Note that internal track number are zero based, but the track variables
// are all 1 based for consistency with "for" statements.
//////////////////////////////////////////////////////////////////////

class TrackSyncMasterVariableType : public ScriptInternalVariable {
  public:
    virtual ~TrackSyncMasterVariableType() {}
    TrackSyncMasterVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

TrackSyncMasterVariableType::TrackSyncMasterVariableType()
{
    setName("trackSyncMaster");
}

void TrackSyncMasterVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    /*
	Track* master = s->getTrackSyncMaster();
	if (master != nullptr)
	  number = master->getDisplayNumber();
    */
    SyncMaster* sm = s->getSyncMaster();
	value->setInt(sm->getTrackSyncMaster());
}

TrackSyncMasterVariableType TrackSyncMasterVariableObj;
ScriptInternalVariable* TrackSyncMasterVariable = &TrackSyncMasterVariableObj;

//////////////////////////////////////////////////////////////////////
//
// outSyncMaster
//
// The number of the track operating as the output sync master,
// 0 if there is no master.
// 
// Note that internal track number are zero based, but the track variables
// are all 1 based for consistency with "for" statements.
//
//////////////////////////////////////////////////////////////////////

class OutSyncMasterVariableType : public ScriptInternalVariable {
  public:
    virtual ~OutSyncMasterVariableType() {}
    OutSyncMasterVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

OutSyncMasterVariableType::OutSyncMasterVariableType()
{
    setName("outSyncMaster");
}

void OutSyncMasterVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    SyncMaster* sm = s->getSyncMaster();
	value->setInt(sm->getTransportMaster());
}

OutSyncMasterVariableType OutSyncMasterVariableObj;
ScriptInternalVariable* OutSyncMasterVariable = &OutSyncMasterVariableObj;

/****************************************************************************
 *                                                                          *
 *   						  COMMON SYNC STATE                             *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// tempo
//
// The current sync tempo.  For Sync=Out this is the tempo we calculated.
// For Sync=In this is the tempo we're smoothing from the external source.
// For Sync=Host this is the tempo reported by the host.
//
//////////////////////////////////////////////////////////////////////

class SyncTempoVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncTempoVariableType() {}
    SyncTempoVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncTempoVariableType::SyncTempoVariableType()
{
    setName("syncTempo");
}

void SyncTempoVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    SyncMaster* sm = s->getSyncMaster();
	float tempo = sm->varGetTempo(SyncSourceTransport);

	// assume its ok to truncate this one, if you want something
	// more accurate could have a RealSyncTempoVariable?
	value->setLong((long)tempo);
}

SyncTempoVariableType SyncTempoVariableObj;
ScriptInternalVariable* SyncTempoVariable = &SyncTempoVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncRawBeat
//
// The current absolute beat count.
// This will be the same as syncOutRawBeat, syncInRawBeat, 
// or syncHostRawBeat depending on the SyncMode of the current track.
//
//////////////////////////////////////////////////////////////////////

class SyncRawBeatVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncRawBeatVariableType() {}
    SyncRawBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncRawBeatVariableType::SyncRawBeatVariableType()
{
    setName("syncRawBeat");
}

void SyncRawBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    SyncMaster* sm = s->getSyncMaster();
    value->setInt(sm->varGetBeat(SyncSourceTransport));
}

SyncRawBeatVariableType SyncRawBeatVariableObj;
ScriptInternalVariable* SyncRawBeatVariable = &SyncRawBeatVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncBeat
//
// The current bar relative beat count.
// This will be the same as syncOutBeat, syncInBeat, or syncHostBeat
// depending on the SyncMode of the current track.
//
//////////////////////////////////////////////////////////////////////

class SyncBeatVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncBeatVariableType() {}
    SyncBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncBeatVariableType::SyncBeatVariableType()
{
    setName("syncBeat");
}

void SyncBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    SyncMaster* sm = s->getSyncMaster();
    value->setInt(sm->varGetBeat(SyncSourceTransport));
}

SyncBeatVariableType SyncBeatVariableObj;
ScriptInternalVariable* SyncBeatVariable = &SyncBeatVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncBar
//
// The current bar count.
// This will be the same as syncOutBar, syncInBar, or syncHostBar
// depending on the SyncMode of the current track.
//
//////////////////////////////////////////////////////////////////////

class SyncBarVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncBarVariableType() {}
    SyncBarVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncBarVariableType::SyncBarVariableType()
{
    setName("syncBar");
}

void SyncBarVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    SyncMaster* sm = s->getSyncMaster();
    value->setInt(sm->varGetBar(SyncSourceTransport));
}

SyncBarVariableType SyncBarVariableObj;
ScriptInternalVariable* SyncBarVariable = &SyncBarVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncPulses
//
// The number of pulses in the sync tracker.
//
// update: lobotomized after the Great Sync Reorganization
//
//////////////////////////////////////////////////////////////////////
#if 0
class SyncPulsesVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncPulsesVariableType() {}
    SyncPulsesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncPulsesVariableType::SyncPulsesVariableType()
{
    setName("syncPulses");
}

void SyncPulsesVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker == nullptr)
      value->setInt(0);
    else {
        // since resizes are deferred until the next pulse, look there first
        value->setInt(tracker->getFutureLoopPulses());
    }
}

SyncPulsesVariableType SyncPulsesVariableObj;
ScriptInternalVariable* SyncPulsesVariable = &SyncPulsesVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncPulse
//
// The current pulse in the sync tracker for this track.
//
// update: lobotomized after the Great Sync Reorganization
//
//////////////////////////////////////////////////////////////////////

class SyncPulseVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncPulseVariableType() {}
    SyncPulseVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncPulseVariableType::SyncPulseVariableType()
{
    setName("syncPulse");
}

void SyncPulseVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != nullptr)
      value->setLong(tracker->getPulse());
    else
      value->setNull();
}

SyncPulseVariableType SyncPulseVariableObj;
ScriptInternalVariable* SyncPulseVariable = &SyncPulseVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncPulseFrames
//
// The length of the sync pulse in frames.
//
//////////////////////////////////////////////////////////////////////

class SyncPulseFramesVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncPulseFramesVariableType() {}
    SyncPulseFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncPulseFramesVariableType::SyncPulseFramesVariableType()
{
    setName("syncPulseFrames");
}

void SyncPulseFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != nullptr)
      value->setFloat(tracker->getPulseFrames());
    else
      value->setNull();
}

SyncPulseFramesVariableType SyncPulseFramesVariableObj;
ScriptInternalVariable* SyncPulseFramesVariable = &SyncPulseFramesVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncLoopFrames
//
// The length of the sync loop in frames.
//
//////////////////////////////////////////////////////////////////////

class SyncLoopFramesVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncLoopFramesVariableType() {}
    SyncLoopFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncLoopFramesVariableType::SyncLoopFramesVariableType()
{
    setName("syncLoopFrames");
}

void SyncLoopFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != nullptr)
      value->setLong(tracker->getFutureLoopFrames());
    else
      value->setNull();
}

SyncLoopFramesVariableType SyncLoopFramesVariableObj;
ScriptInternalVariable* SyncLoopFramesVariable = &SyncLoopFramesVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncAudioFrame
//
// The actual Loop frame at the last pulse.  The difference between
// this and SyncPulseFrame is the amount of drift (after wrapping).
//
//////////////////////////////////////////////////////////////////////

class SyncAudioFrameVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncAudioFrameVariableType() {}
    SyncAudioFrameVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncAudioFrameVariableType::SyncAudioFrameVariableType()
{
    setName("syncAudioFrame");
}

void SyncAudioFrameVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != nullptr)
      value->setLong(tracker->getAudioFrame());
    else
      value->setNull();
}

SyncAudioFrameVariableType SyncAudioFrameVariableObj;
ScriptInternalVariable* SyncAudioFrameVariable = &SyncAudioFrameVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncDrift
//
// The current amount of drift calculated on the last pulse.
// This will be a positive or negative number.
//
//////////////////////////////////////////////////////////////////////

class SyncDriftVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncDriftVariableType() {}
    SyncDriftVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncDriftVariableType::SyncDriftVariableType()
{
    setName("syncDrift");
}

void SyncDriftVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != nullptr)
      value->setLong((long)(tracker->getDrift()));
    else
      value->setNull();
}

SyncDriftVariableType SyncDriftVariableObj;
ScriptInternalVariable* SyncDriftVariable = &SyncDriftVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncAverageDrift
//
// The average amount of drift over the last 96 pulses.
// This may be more accurate than the last drift sometimes though
// we're not using it in the tests.
//
//////////////////////////////////////////////////////////////////////

class SyncAverageDriftVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncAverageDriftVariableType() {}
    SyncAverageDriftVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncAverageDriftVariableType::SyncAverageDriftVariableType()
{
    setName("syncAverageDrift");
}

void SyncAverageDriftVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != nullptr)
      value->setLong((long)(tracker->getAverageDrift()));
    else
      value->setNull();
}

SyncAverageDriftVariableType SyncAverageDriftVariableObj;
ScriptInternalVariable* SyncAverageDriftVariable = &SyncAverageDriftVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncDriftChecks
//
// The number of sync drift checks that have been perfomed with
// this tracker.
//
//////////////////////////////////////////////////////////////////////

class SyncDriftChecksVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncDriftChecksVariableType() {}
    SyncDriftChecksVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncDriftChecksVariableType::SyncDriftChecksVariableType()
{
    setName("syncDriftChecks");
}

void SyncDriftChecksVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != nullptr)
      value->setInt(tracker->getDriftChecks());
    else
      value->setNull();
}

SyncDriftChecksVariableType SyncDriftChecksVariableObj;
ScriptInternalVariable* SyncDriftChecksVariable = &SyncDriftChecksVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncCorrections
//
// The number of sync drift corrections that have been perfomed with
// this tracker.
//
//////////////////////////////////////////////////////////////////////

class SyncCorrectionsVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncCorrectionsVariableType() {}
    SyncCorrectionsVariableType();
    void getTrackValue(Track* t, ExValue* value);
	void setValue(ScriptInterpreter* si, ExValue* value);
};

SyncCorrectionsVariableType::SyncCorrectionsVariableType()
{
    setName("syncCorrections");
}

void SyncCorrectionsVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != nullptr)
      value->setInt(tracker->getDriftCorrections());
    else
      value->setNull();
}

/**
 * This is one of the few variables that has a setter.
 * We allow this so we can force a drift realign, then reset the counter
 * so we can continue testing for zero in other parts of the test.
 */
void SyncCorrectionsVariableType::setValue(ScriptInterpreter* si, 
                                                  ExValue* value)
{
    Track* t = si->getTargetTrack();
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != nullptr)
      tracker->setDriftCorrections(value->getInt());
}

SyncCorrectionsVariableType SyncCorrectionsVariableObj;
ScriptInternalVariable* SyncCorrectionsVariable = &SyncCorrectionsVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncDealign
//
// The number of frames the current track is dealigned from the
// sync tracker for this track.
//
//////////////////////////////////////////////////////////////////////

class SyncDealignVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncDealignVariableType() {}
    SyncDealignVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncDealignVariableType::SyncDealignVariableType()
{
    setName("syncDealign");
}

void SyncDealignVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker == nullptr)
      value->setInt(0);
    else
      value->setLong(tracker->getDealign(t));
}

SyncDealignVariableType SyncDealignVariableObj;
ScriptInternalVariable* SyncDealignVariable = &SyncDealignVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncPreRealignFrame
//
// The Loop frame prior to the last Realign.
//
//////////////////////////////////////////////////////////////////////

class SyncPreRealignFrameVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncPreRealignFrameVariableType() {}
    SyncPreRealignFrameVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncPreRealignFrameVariableType::SyncPreRealignFrameVariableType()
{
    setName("syncPreRealignFrame");
}

void SyncPreRealignFrameVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncState* state = t->getSyncState();
	value->setInt((int)(state->getPreRealignFrame()));
}

SyncPreRealignFrameVariableType SyncPreRealignFrameVariableObj;
ScriptInternalVariable* SyncPreRealignFrameVariable = &SyncPreRealignFrameVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncCyclePulses
//
// The number of external sync pulses counted during recording.
//
//////////////////////////////////////////////////////////////////////

class SyncCyclePulsesVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncCyclePulsesVariableType() {}
    SyncCyclePulsesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncCyclePulsesVariableType::SyncCyclePulsesVariableType()
{
    setName("syncCyclePulses");
}

void SyncCyclePulsesVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncState* state = t->getSyncState();
	value->setInt(state->getCyclePulses());
}

SyncCyclePulsesVariableType SyncCyclePulsesVariableObj;
ScriptInternalVariable* SyncCyclePulsesVariable = &SyncCyclePulsesVariableObj;
#endif

/****************************************************************************
 *                                                                          *
 *   							   OUT SYNC                                 *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// syncOutTempo
//
// The tempo of the internal clock used for out sync.
// This is the same value returned by "tempo" but only if the
// current track is in Sync=Out or Sync=OutUserStart.
//
//////////////////////////////////////////////////////////////////////

class SyncOutTempoVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncOutTempoVariableType() {}
    SyncOutTempoVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncOutTempoVariableType::SyncOutTempoVariableType()
{
    setName("syncOutTempo");
}

void SyncOutTempoVariableType::getTrackValue(Track* t, ExValue* value)
{
	//float tempo = t->getSynchronizer()->getOutTempo();
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
    float tempo = sm->varGetTempo(SyncSourceTransport);
    
	// assume its ok to truncate this one, if you want something
	// more accurate could have a RealTempoVariable?
	value->setLong((long)tempo);
}

SyncOutTempoVariableType SyncOutTempoVariableObj;
ScriptInternalVariable* SyncOutTempoVariable = &SyncOutTempoVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncOutRawBeat
//
// The current raw beat count maintained by the internal clock.
// This will be zero if the internal clock is not running.
//
//////////////////////////////////////////////////////////////////////

class SyncOutRawBeatVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncOutRawBeatVariableType() {}
    SyncOutRawBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncOutRawBeatVariableType::SyncOutRawBeatVariableType()
{
    setName("syncOutRawBeat");
}

void SyncOutRawBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    //value->setInt(s->getOutRawBeat());
    
    SyncMaster* sm = s->getSyncMaster();
    value->setInt(sm->varGetBeat(SyncSourceTransport));
}

SyncOutRawBeatVariableType SyncOutRawBeatVariableObj;
ScriptInternalVariable* SyncOutRawBeatVariable = &SyncOutRawBeatVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncOutBeat
//
// The current beat count maintained by the internal clock,
// relative to the bar.
//
//////////////////////////////////////////////////////////////////////

class SyncOutBeatVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncOutBeatVariableType() {}
    SyncOutBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncOutBeatVariableType::SyncOutBeatVariableType()
{
    setName("syncOutBeat");
}

void SyncOutBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    //value->setInt(s->getOutBeat());
    SyncMaster* sm = s->getSyncMaster();
    value->setInt(sm->varGetBeat(SyncSourceTransport));
}

SyncOutBeatVariableType SyncOutBeatVariableObj;
ScriptInternalVariable* SyncOutBeatVariable = &SyncOutBeatVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncOutBar
//
// The current bar count maintained by the internal clock.
//
//////////////////////////////////////////////////////////////////////

class SyncOutBarVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncOutBarVariableType() {}
    SyncOutBarVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncOutBarVariableType::SyncOutBarVariableType()
{
    setName("syncOutBar");
}

void SyncOutBarVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    //value->setInt(s->getOutBar());
    SyncMaster* sm = s->getSyncMaster();
    value->setInt(sm->varGetBar(SyncSourceTransport));
}

SyncOutBarVariableType SyncOutBarVariableObj;
ScriptInternalVariable* SyncOutBarVariable = &SyncOutBarVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncOutSending
//
// "true" if we are currently sending MIDI clocks, "false" if not.
//
//////////////////////////////////////////////////////////////////////

class SyncOutSendingVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncOutSendingVariableType() {}
    SyncOutSendingVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncOutSendingVariableType::SyncOutSendingVariableType()
{
    setName("syncOutSending");
}

void SyncOutSendingVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
	value->setBool(sm->varIsMidiOutSending());
}

SyncOutSendingVariableType SyncOutSendingVariableObj;
ScriptInternalVariable* SyncOutSendingVariable = &SyncOutSendingVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncOutStarted
//
// "true" if we have send a MIDI Start message, "false" if not.
//
//////////////////////////////////////////////////////////////////////

class SyncOutStartedVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncOutStartedVariableType() {}
    SyncOutStartedVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncOutStartedVariableType::SyncOutStartedVariableType()
{
    setName("syncOutStarted");
}

void SyncOutStartedVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
    // no longer get these from Synchronizer
	//value->setBool(t->getSynchronizer()->isStarted());
    value->setBool(sm->varIsMidiOutStarted());
}

SyncOutStartedVariableType SyncOutStartedVariableObj;
ScriptInternalVariable* SyncOutStartedVariable = &SyncOutStartedVariableObj;

/****************************************************************************
 *                                                                          *
 *   							  MIDI SYNC                                 *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// syncInTempo
//
// The tempo of the external MIDI clock being received.
// This is the same value returned by "tempo" but only if the
// current track SyncMode is In, MIDIBeat, or MIDIBar.
//
//////////////////////////////////////////////////////////////////////

class SyncInTempoVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncInTempoVariableType() {}
    SyncInTempoVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncInTempoVariableType::SyncInTempoVariableType()
{
    setName("syncInTempo");
}

void SyncInTempoVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
	//float tempo = t->getSynchronizer()->getInTempo();
    float tempo = sm->varGetTempo(SyncSourceMidi);
    
	// assume its ok to truncate this one, if you want something
	// more accurate could have a RealTempoVariable?
	value->setLong((long)tempo);
}

SyncInTempoVariableType SyncInTempoVariableObj;
ScriptInternalVariable* SyncInTempoVariable = &SyncInTempoVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncInRawBeat
//
// The current beat count derived from the external MIDI clock.
//
//////////////////////////////////////////////////////////////////////

class SyncInRawBeatVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncInRawBeatVariableType() {}
    SyncInRawBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncInRawBeatVariableType::SyncInRawBeatVariableType()
{
    setName("syncInRawBeat");
}

void SyncInRawBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
    value->setInt(sm->varGetMidiInRawBeat());
}

SyncInRawBeatVariableType SyncInRawBeatVariableObj;
ScriptInternalVariable* SyncInRawBeatVariable = &SyncInRawBeatVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncInBeat
//
// The current beat count derived from the external MIDI clock,
// relative to the bar.
//
//////////////////////////////////////////////////////////////////////

class SyncInBeatVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncInBeatVariableType() {}
    SyncInBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncInBeatVariableType::SyncInBeatVariableType()
{
    setName("syncInBeat");
}

void SyncInBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
    value->setInt(sm->varGetMidiInRawBeat());
}

SyncInBeatVariableType SyncInBeatVariableObj;
ScriptInternalVariable* SyncInBeatVariable = &SyncInBeatVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncInBar
//
// The current bar count derived from the external MIDI clock.
//
//////////////////////////////////////////////////////////////////////

class SyncInBarVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncInBarVariableType() {}
    SyncInBarVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncInBarVariableType::SyncInBarVariableType()
{
    setName("syncInBar");
}

void SyncInBarVariableType::getTrackValue(Track* t, ExValue* value)
{
    (void)t;
    //SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
    // not counting these, need to!
    value->setInt(1);
}

SyncInBarVariableType SyncInBarVariableObj;
ScriptInternalVariable* SyncInBarVariable = &SyncInBarVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncInReceiving
//
// True if we are currently receiving MIDI clocks.
//
//////////////////////////////////////////////////////////////////////

class SyncInReceivingVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncInReceivingVariableType() {}
    SyncInReceivingVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncInReceivingVariableType::SyncInReceivingVariableType()
{
    setName("syncInReceiving");
}

void SyncInReceivingVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
	value->setBool(sm->varIsMidiInReceiving());
}

SyncInReceivingVariableType SyncInReceivingVariableObj;
ScriptInternalVariable* SyncInReceivingVariable = &SyncInReceivingVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncInStarted
//
// True if we have received a MIDI start or continue message.
//
//////////////////////////////////////////////////////////////////////

class SyncInStartedVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncInStartedVariableType() {}
    SyncInStartedVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncInStartedVariableType::SyncInStartedVariableType()
{
    setName("syncInStarted");
}

void SyncInStartedVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
	value->setBool(sm->varIsMidiInStarted());
}

SyncInStartedVariableType SyncInStartedVariableObj;
ScriptInternalVariable* SyncInStartedVariable = &SyncInStartedVariableObj;

/****************************************************************************
 *                                                                          *
 *   							  HOST SYNC                                 *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// syncHostTempo
//
// The tempo advertised by the plugin host.
//
//////////////////////////////////////////////////////////////////////

class SyncHostTempoVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncHostTempoVariableType() {}
    SyncHostTempoVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncHostTempoVariableType::SyncHostTempoVariableType()
{
    setName("syncHostTempo");
}

void SyncHostTempoVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
	float tempo = sm->varGetTempo(SyncSourceHost);

	// assume its ok to truncate this one, if you want something
	// more accurate could have a RealTempoVariable?
	value->setLong((long)tempo);
}

SyncHostTempoVariableType SyncHostTempoVariableObj;
ScriptInternalVariable* SyncHostTempoVariable = &SyncHostTempoVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncHostRawBeat
//
// The current beat count given by the host.
//
//////////////////////////////////////////////////////////////////////

class SyncHostRawBeatVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncHostRawBeatVariableType() {}
    SyncHostRawBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncHostRawBeatVariableType::SyncHostRawBeatVariableType()
{
    setName("syncHostRawBeat");
}

void SyncHostRawBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
    value->setInt(sm->varGetBeat(SyncSourceHost));
}

SyncHostRawBeatVariableType SyncHostRawBeatVariableObj;
ScriptInternalVariable* SyncHostRawBeatVariable = &SyncHostRawBeatVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncHostBeat
//
// The current beat count given by the host, relative to the bar.
//
//////////////////////////////////////////////////////////////////////

class SyncHostBeatVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncHostBeatVariableType() {}
    SyncHostBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncHostBeatVariableType::SyncHostBeatVariableType()
{
    setName("syncHostBeat");
}

void SyncHostBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
    value->setInt(sm->varGetBeat(SyncSourceHost));
}

SyncHostBeatVariableType SyncHostBeatVariableObj;
ScriptInternalVariable* SyncHostBeatVariable = &SyncHostBeatVariableObj;

//////////////////////////////////////////////////////////////////////
//
// syncHostBar
//
// The current bar count given by the host.
//
//////////////////////////////////////////////////////////////////////

class SyncHostBarVariableType : public ScriptInternalVariable {
  public:
    virtual ~SyncHostBarVariableType() {}
    SyncHostBarVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncHostBarVariableType::SyncHostBarVariableType()
{
    setName("syncHostBar");
}

void SyncHostBarVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncMaster* sm = t->getSynchronizer()->getSyncMaster();
    value->setInt(sm->varGetBar(SyncSourceHost));
}

SyncHostBarVariableType SyncHostBarVariableObj;
ScriptInternalVariable* SyncHostBarVariable = &SyncHostBarVariableObj;

/****************************************************************************
 *                                                                          *
 *                                INSTALLATION                              *
 *                                                                          *
 ****************************************************************************/

// ?? What used these?
// I can't imagine why you would need these, and even if that could
// be passed down, it would be improper to use it in scripts?

//////////////////////////////////////////////////////////////////////
//
// installationDirectory
//
// Base directory where Mobius has been installed.
// Typically c:\Program Files\Mobius on Windows and
// /Applications/Mobius on Mac.
//
//////////////////////////////////////////////////////////////////////

class InstallationDirectoryVariableType : public ScriptInternalVariable {
  public:
    virtual ~InstallationDirectoryVariableType() {}
    InstallationDirectoryVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

InstallationDirectoryVariableType::InstallationDirectoryVariableType()
{
    setName("installationDirectory");
}

void InstallationDirectoryVariableType::getValue(ScriptInterpreter* si, 
                                                 ExValue* value)
{
    (void)si;
    //Mobius* m = si->getMobius();
    //MobiusContext* mc = m->getContext();
    //value->setString(mc->getInstallationDirectory());
    value->setNull();
}

InstallationDirectoryVariableType InstallationDirectoryVariableObj;
ScriptInternalVariable* InstallationDirectoryVariable = &InstallationDirectoryVariableObj;

//////////////////////////////////////////////////////////////////////
//
// configurationDirectory
//
// Base directory where Mobius has been installed.
// Typically c:\Program Files\Mobius on Windows and
// /Applications/Mobius on Mac.
//
//////////////////////////////////////////////////////////////////////

class ConfigurationDirectoryVariableType : public ScriptInternalVariable {
  public:
    virtual ~ConfigurationDirectoryVariableType() {}
    ConfigurationDirectoryVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

ConfigurationDirectoryVariableType::ConfigurationDirectoryVariableType()
{
    setName("configurationDirectory");
}

void ConfigurationDirectoryVariableType::getValue(ScriptInterpreter* si, 
                                                 ExValue* value)
{
    (void)si;
    //Mobius* m = si->getMobius();
    //MobiusContext* mc = m->getContext();
    //value->setString(mc->getConfigurationDirectory());
    value->setNull();
}

ConfigurationDirectoryVariableType ConfigurationDirectoryVariableObj;
ScriptInternalVariable* ConfigurationDirectoryVariable = &ConfigurationDirectoryVariableObj;

/****************************************************************************
 *                                                                          *
 *   							 COLLECTIONS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * The collection of all internal variables.
 */
ScriptInternalVariable* InternalVariables[] = {

	// Script state

	SustainCountVariable,
	ClickCountVariable,
	TriggerNumberVariable,
	TriggerValueVariable,
	TriggerOffsetVariable,
	MidiTypeVariable,
	MidiChannelVariable,
	MidiNumberVariable,
	MidiValueVariable,
	ReturnCodeVariable,

	// Special runtime parameters
	NoExternalAudioVariable,

	// Internal State

    BlockFramesVariable,
    SampleFramesVariable,
	
	// Loop sizes
	
	LoopCountVariable,
	LoopNumberVariable,
	LoopFramesVariable,
	LoopFrameVariable,

	CycleCountVariable,
	CycleNumberVariable,
	CycleFramesVariable,
	CycleFrameVariable,

	SubCycleCountVariable,
	SubCycleNumberVariable,
	SubCycleFramesVariable,
	SubCycleFrameVariable,

	LayerCountVariable,
	RedoCountVariable,
	EffectiveFeedbackVariable,
    HistoryFramesVariable,

	// Loop events

	NextEventVariable,
	NextEventFunctionVariable,
	NextLoopVariable,
    EventSummaryVariable,

	// Loop modes

	ModeVariable,
    IsRecordingVariable,
	InOverdubVariable,
	InHalfspeedVariable,
	InReverseVariable,
	InMuteVariable,
	InPauseVariable,
	InRealignVariable,
	InReturnVariable,
	RateVariable,
	RawSpeedVariable,
	RawPitchVariable,
	SpeedToggleVariable,
    SpeedSequenceIndexVariable,
    PitchSequenceIndexVariable,
    WindowOffsetVariable,

	// Track state

	TrackCountVariable,
	TrackVariable,
	GlobalMuteVariable,
	SoloVariable,
	TrackSyncMasterVariable,
	OutSyncMasterVariable,

	// Generic Sync

	//SyncAudioFrameVariable,
    SyncBarVariable,
    SyncBeatVariable,
	//SyncCorrectionsVariable,
	//SyncCyclePulsesVariable,
	//SyncDealignVariable,
	//SyncDriftVariable,
	//SyncDriftChecksVariable,
    //SyncLoopFramesVariable,
	//SyncPreRealignFrameVariable,
	//SyncPulseVariable,
	//SyncPulseFramesVariable,
    //SyncPulsesVariable,
    SyncRawBeatVariable,
    SyncTempoVariable,

	// Out Sync

	SyncOutTempoVariable,
	SyncOutRawBeatVariable,
	SyncOutBeatVariable,
	SyncOutBarVariable,
	SyncOutSendingVariable,
	SyncOutStartedVariable,

	// MIDI Sync

	SyncInTempoVariable,
	SyncInRawBeatVariable,
	SyncInBeatVariable,
	SyncInBarVariable,
	SyncInReceivingVariable,
	SyncInStartedVariable,

	// Host sync

	SyncHostTempoVariable,
	SyncHostRawBeatVariable,
	SyncHostBeatVariable,
	SyncHostBarVariable,

    // Installation

    InstallationDirectoryVariable,
    ConfigurationDirectoryVariable,

    nullptr
};

/**
 * Lookup an internal variable during script parsing.
 */
ScriptInternalVariable* 
ScriptInternalVariable::getVariable(const char* name)
{
    ScriptInternalVariable* found = nullptr;
    for (int i = 0 ; InternalVariables[i] != nullptr ; i++) {
		ScriptInternalVariable* v = InternalVariables[i];
		if (v->isMatch(name)) {
			found = v;
			break;
		}
	}
    return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
