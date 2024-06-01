/*
 * System constants that define the types of events
 * that can be scheduled internally on the loop timeline
 * and returned in the MobiusState.
 *
 * Since these are spread all over the old code I'm not exactly
 * sure what the internal/display names are so guess for now
 * and only require the display name.  Not even sure if we really
 * need the difference since these are not looked up by name.
 */

#include <vector>

#include "SystemConstant.h"
#include "UIEventType.h"

std::vector<UIEventType*> UIEventType::Instances;

UIEventType::UIEventType(const char* name, const char* symbol) :
    SystemConstant(name, nullptr)
{
    ordinal = (int)Instances.size();
    Instances.push_back(this);
    
    // if the symbol is not set, this is hidden
    timelineSymbol = symbol;
    isStart = false;
    isEnd = false;
    isWeird = false;
}

UIEventType::UIEventType(const char* name, const char* symbol, bool s, bool e, bool w) :
    UIEventType(name, symbol)
{
    isStart = s;
    isEnd = e;
    isWeird = w;
}

UIEventType* UIEventType::find(const char* name)
{
    UIEventType* found = nullptr;
    for (int i = 0 ; i < Instances.size() ; i++) {
        UIEventType* t = Instances[i];
        if (strcmp(name, t->name) == 0) {
            found = t;
            break;
        }
    }
    return found;
}

// usualy ugly SystemConstant pointer dance for auto-free
// to avoid link conflicts with the same objects in old code
// use the Type suffix, but should really decide if we need these at all
// don't know if the internal names match since they're hard to pin down

// shouldn't try to display these on the timeline
UIEventType UIInvokeEventObj {"Invoke", "?"};
UIEventType* UIInvokeEventType = &UIInvokeEventObj;

UIEventType UIValidateEventObj {"Validate", "V"};
UIEventType* UIValidateEventType = &UIValidateEventObj;


UIEventType UIRecordEventObj {"Record", "R", true, false, false};
UIEventType* UIRecordEventType = &UIRecordEventObj;

// same symbol as Record, but isEnd will color it red
UIEventType UIRecordStopEventObj {"RecordStop", "R", false, true, false};
UIEventType* UIRecordStopEventType = &UIRecordStopEventObj;

UIEventType UIPlayEventObj {"Play", "P"};
UIEventType* UIPlayEventType = &UIPlayEventObj;

UIEventType UIOverdubEventObj {"Overdub", "O"};
UIEventType* UIOverdubEventType = &UIOverdubEventObj;

UIEventType UIMultiplyEventObj {"Multiply", "M", true, false, false};
UIEventType* UIMultiplyEventType = &UIMultiplyEventObj;

UIEventType UIMultiplyEndEventObj {"MultiplyEnd", "M", false, true, false};
UIEventType* UIMultiplyEndEventType = &UIMultiplyEndEventObj;

UIEventType UIInstantMultiplyEventObj {"InstantMultiply", "IM"};
UIEventType* UIInstantMultiplyEventType = &UIInstantMultiplyEventObj;

UIEventType UIInstantDivideEventObj {"InstantDivide", "ID"};
UIEventType* UIInstantDivideEventType = &UIInstantDivideEventObj;

UIEventType UIInsertEventObj {"Insert", "I", true, false, false};
UIEventType* UIInsertEventType = &UIInsertEventObj;

UIEventType UIInsertEndEventObj {"InsertEnd", "I", false, true, false};
UIEventType* UIInsertEndEventType = &UIInsertEndEventObj;

UIEventType UIStutterEventObj {"Stutter", "St"};
UIEventType* UIStutterEventType = &UIStutterEventObj;

UIEventType UIReplaceEventObj {"Replace", "Rp"};
UIEventType* UIReplaceEventType = &UIReplaceEventObj;

UIEventType UISubstituteEventObj {"Substitute", "S"};
UIEventType* UISubstituteEventType = &UISubstituteEventObj;

// I think the next three are internal only
UIEventType UILoopEventObj {"Loop", "?"};
UIEventType* UILoopEventType = &UILoopEventObj;

UIEventType UICycleEventObj {"Cycle", "?"};
UIEventType* UICycleEventType = &UICycleEventObj;

UIEventType UISubCycleEventObj {"Subcycle", "?"};
UIEventType* UISubCycleEventType = &UISubCycleEventObj;

UIEventType UIReverseEventObj {"Reverse", "Rv"};
UIEventType* UIReverseEventType = &UIReverseEventObj;

// I think internal due to latency compensation
UIEventType UIReversePlayEventObj {"ReversePlay", "?"};
UIEventType* UIReversePlayEventType = &UIReversePlayEventObj;

UIEventType UISpeedEventObj {"Speed", "Sp"};
UIEventType* UISpeedEventType = &UISpeedEventObj;

UIEventType UIRateEventObj {"Rate", "Ra"};
UIEventType* UIRateEventType = &UIRateEventObj;

UIEventType UIPitchEventObj {"Pitch", "Pi"};
UIEventType* UIPitchEventType = &UIPitchEventObj;

UIEventType UIBounceEventObj {"Bounce", "B"};
UIEventType* UIBounceEventType = &UIBounceEventObj;

UIEventType UIMuteEventObj {"Mute", "Mu"};
UIEventType* UIMuteEventType = &UIMuteEventObj;

// should be filtered
UIEventType UIJumpPlayEventObj {"Jump", "J"};
UIEventType* UIJumpPlayEventType = &UIJumpPlayEventObj;

UIEventType UIUndoEventObj {"Undo", "U"};
UIEventType* UIUndoEventType = &UIUndoEventObj;

UIEventType UIRedoEventObj {"Redo", "Re"};
UIEventType* UIRedoEventType = &UIRedoEventObj;

// how does this differ from RunScriptEventObj?
UIEventType UIScriptEventObj {"Script", "Sc", false, false, true};
UIEventType* UIScriptEventType = &UIScriptEventObj;

UIEventType UIStartPointEventObj {"StartPoint", "SP"};
UIEventType* UIStartPointEventType = &UIStartPointEventObj;

UIEventType UIRealignEventObj {"Realign", "Rl"};
UIEventType* UIRealignEventType = &UIRealignEventObj;

// probably only in scripts, but might be nice to see
UIEventType UIMidiStartEventObj {"MIDIStart", "Ms"};
UIEventType* UIMidiStartEventType = &UIMidiStartEventObj;

// these are common and really need an icon
UIEventType UISwitchEventObj {"Switch", "LS"};
UIEventType* UISwitchEventType = &UISwitchEventObj;

UIEventType UIReturnEventObj {"Return", "Rt"};
UIEventType* UIReturnEventType = &UIReturnEventObj;

// weird, I guess paired with ReturnEvent?
UIEventType UISUSReturnEventObj {"SUSReturn", "Rt", false, false, true};
UIEventType* UISUSReturnEventType = &UISUSReturnEventObj;

// pretty sure these are instant
UIEventType UITrackEventObj {"Track", "Tk"};
UIEventType* UITrackEventType = &UITrackEventObj;

// would be nice to capture the Script name in the event summary
// for the extended display
// wait, how does this differ from just ScriptEvent?
UIEventType UIRunScriptEventObj {"Script", "Sc", false, false, true};
UIEventType* UIRunScriptEventType = &UIRunScriptEventObj;

UIEventType UISampleTriggerEventObj {"Sample", "Sm"};
UIEventType* UISampleTriggerEventType = &UISampleTriggerEventObj;

// not sure if these can happen
UIEventType UISyncEventObj {"Sync", "Sy"};
UIEventType* UISyncEventType = &UISyncEventObj;

UIEventType UISlipEventObj {"Slip", "Sl"};
UIEventType* UISlipEventType = &UISlipEventObj;

UIEventType UIMoveEventObj {"Move", "Mv"};
UIEventType* UIMoveEventType = &UIMoveEventObj;

UIEventType UIShuffleEventObj {"Shuffle", "Sh"};
UIEventType* UIShuffleEventType = &UIShuffleEventObj;

// I think just somethign I use for debugging
UIEventType UISyncCheckEventObj {"SyncCheck", "?"};
UIEventType* UISyncCheckEventType = &UISyncCheckEventObj;

UIEventType UIMidiOutEventObj {"MIDIOut", "Mo"};
UIEventType* UIMidiOutEventType = &UIMidiOutEventObj;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
