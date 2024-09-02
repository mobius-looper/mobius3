/*
 * System constants that define the types of events
 * that can be scheduled internally on the loop timeline.
 * No behavior is defined here, only that necessary for
 * showing them in the UI.
 *
 * In old code the class is EventType and the static
 * objects are strewn about all over, typically with their
 * Function definitions.  Ordinal mapping will be harder for these.
 *
 * Need to use the UI prefix for object pointer names to avoid conflict.
 */

#pragma once

#include <vector>
#include "SystemConstant.h"

class UIEventType : public SystemConstant
{
  public:

    static std::vector<UIEventType*> Instances;
    static UIEventType* find(const char* name);
    
    UIEventType(const char* name, const char* displayName);
    UIEventType(const char* name, const char* displayName, bool start, bool end, bool weird);
    virtual ~UIEventType() {};

    // Characters used to represent this on the loop status timeline
    const char* timelineSymbol;

    // todo: will probably want refernces to icons at some point

    // when there is a start end pair, indiciates this is the start event
    bool isStart;
    bool isEnd;

    // flag I want to set for events I don't understand so we can
    // color them different if they happen
    bool isWeird;
    
};

/*
 * this is all of the types that the engine uses, as we progress determine
 * how many of these really need to be exposed to the UI.
 * Do we need really need pointer constants for these in the UI?
 * They'll always just be returned in MobiusState
 */

extern UIEventType* UIInvokeEventType;
extern UIEventType* UIValidateEventType;
extern UIEventType* UIRecordEventType;
extern UIEventType* UIRecordStopEventType;
extern UIEventType* UIPlayEventType;
extern UIEventType* UIOverdubEventType;
extern UIEventType* UIMultiplyEventType;
extern UIEventType* UIMultiplyEndEventType;
extern UIEventType* UIInstantMultiplyEventType;
extern UIEventType* UIInstantDivideEventType;
extern UIEventType* UIInsertEventType;
extern UIEventType* UIInsertEndEventType;
extern UIEventType* UIStutterEventType;
extern UIEventType* UIReplaceEventType;
extern UIEventType* UISubstituteEventType;
extern UIEventType* UILoopEventType;
extern UIEventType* UICycleEventType;
extern UIEventType* UISubCycleEventType;
extern UIEventType* UIReverseEventType;
extern UIEventType* UIReversePlayEventType;
extern UIEventType* UISpeedEventType;
extern UIEventType* UIRateEventType;
extern UIEventType* UIPitchEventType;
extern UIEventType* UIBounceEventType;
extern UIEventType* UIMuteEventType;
extern UIEventType* UIPlayEventType;
extern UIEventType* UIJumpPlayEventType;
extern UIEventType* UIUndoEventType;
extern UIEventType* UIRedoEventType;
extern UIEventType* UIScriptEventType;
extern UIEventType* UIStartPointEventType;
extern UIEventType* UIRealignEventType;
extern UIEventType* UIMidiStartEventType;
extern UIEventType* UISwitchEventType;
extern UIEventType* UIReturnEventType;
extern UIEventType* UISUSReturnEventType;
extern UIEventType* UITrackEventType;
extern UIEventType* UIRunScriptEventType;
extern UIEventType* UISampleTriggerEventType;
extern UIEventType* UISyncEventType;
extern UIEventType* UISlipEventType;
extern UIEventType* UIMoveEventType;
extern UIEventType* UIShuffleEventType;
extern UIEventType* UIMidiOutEventType;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
