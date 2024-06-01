/**
 * Model for function definitions.
 *
 */

#include <vector>

#include "../util/Trace.h"
#include "../util/Util.h"

#include "SystemConstant.h"
#include "FunctionDefinition.h"

//////////////////////////////////////////////////////////////////////
//
// Global Registry
//
//////////////////////////////////////////////////////////////////////

/**
 * A registry of all Functions, created as they are constructed.
 * This is primarily for binding where we need to associate things
 * dynamically with any parameter identified by name.  Engine
 * code rarely needs these.
 *
 * The vector will be built out by the Function constructor.
 * Normally all Function objects will be static objects.
 *
 * update: With the introduction of the SymbolTable the only place this
 * is necessary is when building the SymbolTable.  Consider other
 * ways of doing this so we can get rid of the static vector.
 */
std::vector<FunctionDefinition*> FunctionDefinition::Instances;

void FunctionDefinition::trace()
{
    for (int i = 0 ; i < Instances.size() ; i++) {
        FunctionDefinition* f = Instances[i];
        Trace(1, "Function %s\n", f->getName());
    }
}

/**
 * Find a Function by name
 * This doesn't happen often so we can do a liner search.
 */
FunctionDefinition* FunctionDefinition::find(const char* name)
{
	FunctionDefinition* found = nullptr;
	
	for (int i = 0 ; i < Instances.size() ; i++) {
		FunctionDefinition* f = Instances[i];
		if (StringEqualNoCase(f->getName(), name)) {
            found = f;
            break;
        }
	}
	return found;
}

//////////////////////////////////////////////////////////////////////
//
// Class
//
//////////////////////////////////////////////////////////////////////

FunctionDefinition::FunctionDefinition(const char* name) :
    SystemConstant(name, nullptr)
{
    // add to the global registry
    ordinal = (int)Instances.size();
    Instances.push_back(this);

    // todo, will want to flag deprecated functions
    // or better yet, just leave them out
}

FunctionDefinition::~FunctionDefinition()
{
}

//////////////////////////////////////////////////////////////////////
//
// Function Definition Objects
//
// Unlike Parameter objects, we didn't keep these in a single file,
// they were strewn about the code in files with other things related to
// the implementation of that function.
//
// Since there is almost no implementation in these we don't need to
// subclass them and can just make static objects direcdtly from the base class.
//
// They have historically not has display names, just use a nice name for them
// The old code had static pointers for these, RecordFunction etc.
// Try to avoid that in the UI and see if we can just look them up by name
// since unlike Parameter there are rarely used in code.
//
//////////////////////////////////////////////////////////////////////

FunctionDefinition AutoRecordDef {"AutoRecord"};
FunctionDefinition BackwardDef {"Backward"};
FunctionDefinition BounceDef {"Bounce"};
FunctionDefinition CheckpointDef {"Checkpoint"};
FunctionDefinition ClearDef {"Clear"};
FunctionDefinition ConfirmDef {"Confirm"};
FunctionDefinition DivideDef {"Divide"};
FunctionDefinition Divide3Def {"Divide3"};
FunctionDefinition Divide4Def {"Divide4"};
FunctionDefinition FocusLockDef {"FocusLock"};
FunctionDefinition ForwardDef {"Forward"};
FunctionDefinition GlobalMuteDef {"GlobalMute"};
FunctionDefinition GlobalPauseDef {"GlobalPause"};
FunctionDefinition GlobalResetDef {"GlobalReset"};
FunctionDefinition HalfspeedDef {"Halfspeed"};
FunctionDefinition InsertDef {"Insert"};
FunctionDefinition InstantMultiplyDef {"InstantMultiply"};
// these are similar to replicated functions but have been in use
// for a long time, think about this
FunctionDefinition InstantMultiply3Def {"InstantMultiply3"};
FunctionDefinition InstantMultiply4Def {"InstantMultiply4"};

// Formerly LoopN, Loop1, Loop2, etc.
FunctionDefinition SelectLoopDef {"SelectLoop"};

FunctionDefinition MidiStartDef {"MidiStart"};
FunctionDefinition MidiStopDef {"MidiStop"};

FunctionDefinition MultiplyDef {"Multiply"};
FunctionDefinition MuteDef {"Mute"};
FunctionDefinition MuteRealignDef {"MuteRealign"};
FunctionDefinition MuteMidiStartDef {"MuteMidiStart"};
FunctionDefinition NextLoopDef {"NextLoop"};
FunctionDefinition NextTrackDef {"NextTrack"};
FunctionDefinition OverdubDef {"Overdub"};
FunctionDefinition PauseDef {"Pause"};
FunctionDefinition PitchDownDef {"PitchDown"};
FunctionDefinition PitchNextDef {"PitchNext"};
FunctionDefinition PitchCancelDef {"PitchCancel"};
FunctionDefinition PitchPrevDef {"PitchPrev"};
FunctionDefinition PitchStepDef {"PitchStep"};
FunctionDefinition PitchUpDef {"PitchUp"};
FunctionDefinition PlayDef {"Play"};
FunctionDefinition PrevLoopDef {"PrevLoop"};
FunctionDefinition PrevTrackDef {"PrevTrack"};
FunctionDefinition RealignDef {"Realign"};
FunctionDefinition RecordDef {"Record"};
FunctionDefinition RedoDef {"Redo"};
FunctionDefinition RehearseDef {"Rehearse"};
FunctionDefinition ReplaceDef {"Replace"};
FunctionDefinition ResetDef {"Reset"};
FunctionDefinition RestartDef {"Restart"};
FunctionDefinition RestartOnceDef {"RestartOnce"};
FunctionDefinition ReverseDef {"Reverse"};
FunctionDefinition SaveCaptureDef {"SaveCapture"};
FunctionDefinition SaveLoopDef {"SaveLoop"};
FunctionDefinition ShuffleDef {"Shuffle"};
FunctionDefinition SlipForwardDef {"SlipForward"};
FunctionDefinition SlipBackwardDef {"SlipBackward"};
FunctionDefinition SoloDef {"Solo"};
FunctionDefinition SpeedDownDef {"SpeedDown"};
FunctionDefinition SpeedNextDef {"SpeedNext"};
FunctionDefinition SpeedCancelDef {"SpeedCancel"};
FunctionDefinition SpeedPrevDef {"SpeedPrev"};
FunctionDefinition SpeedStepDef {"SpeedStep"};
FunctionDefinition SpeedUpDef {"SpeedUp"};
FunctionDefinition SpeedToggleDef {"SpeedToggle"};
FunctionDefinition StartCaptureDef {"StartCapture"};
FunctionDefinition StartPointDef {"StartPoint"};
FunctionDefinition StopCaptureDef {"StopCapture"};
FunctionDefinition StutterDef {"Stutter"};
FunctionDefinition SubstituteDef {"Substitute"};

//Function SurfaceDef {"Surface"};
//Function* Surface = &SurfaceDef;

// don't really like needing SUS variants for these
// try to just have the base Function with canSustain set
// and make it nice in the binding UI
FunctionDefinition SUSInsertDef {"SUSInsert"};
FunctionDefinition SUSMultiplyDef {"SUSMultiply"};
FunctionDefinition SUSMuteDef {"SUSMute"};
FunctionDefinition SUSMuteRestartDef {"SUSMuteRestart"};
FunctionDefinition SUSNextLoopDef {"SUSNextLoop"};
FunctionDefinition SUSOverdubDef {"SUSOverdub"};
FunctionDefinition SUSPrevLoopDef {"SUSPrevLoop"};
FunctionDefinition SUSRecordDef {"SUSRecord"};
FunctionDefinition SUSReplaceDef {"SUSReplace"};
FunctionDefinition SUSReverseDef {"SUSReverse"};
FunctionDefinition SUSSpeedToggleDef {"SUSSpeedToggle"};
FunctionDefinition SUSStutterDef {"SUSStutter"};
FunctionDefinition SUSSubstituteDef {"SUSSubstitute"};
FunctionDefinition SUSUnroundedInsertDef {"SUSUnroundedInsert"};
FunctionDefinition SUSUnroundedMultiplyDef {"SUSUnroundedMultiply"};

FunctionDefinition SyncStartPointDef {"SyncStartPoint"};
// Formerly TrackN, Track1, etc.
FunctionDefinition SelectTrackDef {"SelectTrack"};
FunctionDefinition TrackCopyDef {"TrackCopy"};
FunctionDefinition TrackCopyTimingDef {"TrackCopyTiming"};
FunctionDefinition TrackGroupDef {"TrackGroup"};
FunctionDefinition TrackResetDef {"TrackReset"};
FunctionDefinition TrimEndDef {"TrimEnd"};
FunctionDefinition TrimStartDef {"TrimStart"};
FunctionDefinition UndoDef {"Undo"};
FunctionDefinition WindowBackwardDef {"WindowBackward"};
FunctionDefinition WindowForwardDef {"WindowForward"};
FunctionDefinition WindowStartBackwardDef {"WindowStartBackward"};
FunctionDefinition WindowStartForwardDef {"WindowStartForward"};
FunctionDefinition WindowEndBackwardDef {"WindowEndBackward"};
FunctionDefinition WindowEndForwardDef {"WindowEndForward"};

// various diagnostic functions for testing
FunctionDefinition TraceStatusDef {"TraceStatus"};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
