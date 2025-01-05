/**
 * Base model for major operating modes within the engine.
 * These are part of the MobiusState model.
 * A track will always be in one of these modes.
 *
 * The engine model uses an internal/display name pair withh
 * a usually lowercase internal name and an upcased display name.
 * Here we'll just use the upcase name.
 *
 * Think about this!!
 * We don't need definition objects at all in the UI, all we care about
 * is the name to show.  Functions should be free to invent any mode
 * they want without having to model something back in the UI layer.
 * MobiusState could just keep a buffer to hold the mode name string
 * but I don't want to mess with string copying.
 *
 * It would work to have MobiusState just have a SystemConstant*
 * to whatever the internal object is and get the name there.
 * Leave this blown out for now in case there is something
 * else of interest we want to capture about modes besides just the name.
 */

#include "../util/Trace.h"
#include "../util/Util.h"

#include "SystemConstant.h"
#include "ModeDefinition.h"

//////////////////////////////////////////////////////////////////////
//
// Global Registry
//
//////////////////////////////////////////////////////////////////////

/**
 * A registry of all modes, created as they are constructed.
 * Probably not necessary since modes are returned in MobiusState
 * we don't need to look them up by name.  This does also provide
 * the assignment of ordinals however.
 */
std::vector<ModeDefinition*> ModeDefinition::Instances;

void ModeDefinition::trace()
{
    for (int i = 0 ; i < Instances.size() ; i++) {
        ModeDefinition* m = Instances[i];
        Trace(2, "Mode %s\n", m->getName());
    }
}

/**
 * Find a Function by name
 * This doesn't happen often so we can do a liner search.
 */
ModeDefinition* ModeDefinition::find(const char* name)
{
	ModeDefinition* found = nullptr;
	
	for (int i = 0 ; i < Instances.size() ; i++) {
		ModeDefinition* m = Instances[i];
		if (StringEqualNoCase(m->getName(), name)) {
            found = m;
            break;
        }
	}
	return found;
}

//////////////////////////////////////////////////////////////////////
//
// Base Class
//
//////////////////////////////////////////////////////////////////////

ModeDefinition::ModeDefinition(const char* name) :
    SystemConstant(name, nullptr)
{
    // add to the global registry
    ordinal = (int)Instances.size();
    Instances.push_back(this);

    // todo, will want to flag deprecated functions
    // or better yet, just leave them out
}

ModeDefinition::~ModeDefinition()
{
}

//////////////////////////////////////////////////////////////////////
//
// Mode Definition Objects
//
//////////////////////////////////////////////////////////////////////

//
// Major Modes
//

// not technically a Loop mode, the mode the engine is
// logically in when all theh tracks are in Reset
ModeDefinition GlobalResetModeDef {"Global Reset"};
ModeDefinition* UIGlobalResetMode = &GlobalResetModeDef;

ModeDefinition ConfirmModeDef {"Confirm"};
ModeDefinition* UIConfirmMode = &ConfirmModeDef;

ModeDefinition InsertModeDef {"Insert"};
ModeDefinition* UIInsertMode = &InsertModeDef;

ModeDefinition MultiplyModeDef {"Multiply"};
ModeDefinition* UIMultiplyMode = &MultiplyModeDef;

ModeDefinition MuteModeDef {"Mute"};
ModeDefinition* UIMuteMode = &MuteModeDef;

ModeDefinition OverdubModeDef {"Overdub"};
ModeDefinition* UIOverdubMode = &OverdubModeDef;

ModeDefinition PauseModeDef {"Pause"};
ModeDefinition* UIPauseMode = &PauseModeDef;

ModeDefinition PlayModeDef {"Play"};
ModeDefinition* UIPlayMode = &PlayModeDef;

ModeDefinition RecordModeDef {"Record"};
ModeDefinition* UIRecordMode = &RecordModeDef;

ModeDefinition RehearseModeDef {"Rehearse"};
ModeDefinition* UIRehearseMode = &RehearseModeDef;

ModeDefinition RehearseRecordModeDef {"RehearseRecord"};
ModeDefinition* UIRehearseRecordMode = &RehearseRecordModeDef;

ModeDefinition ReplaceModeDef {"Replace"};
ModeDefinition* UIReplaceMode = &ReplaceModeDef;

ModeDefinition ResetModeDef {"Reset"};
ModeDefinition* UIResetMode = &ResetModeDef;

ModeDefinition RunModeDef {"Run"};
ModeDefinition* UIRunMode = &RunModeDef;

ModeDefinition StutterModeDef {"Stutter"};
ModeDefinition* UIStutterMode = &StutterModeDef;

ModeDefinition SubstituteModeDef {"Substitute"};
ModeDefinition* UISubstituteMode = &SubstituteModeDef;

ModeDefinition SwitchModeDef {"Switch"};
ModeDefinition* UISwitchMode = &SwitchModeDef;

ModeDefinition SynchronizeModeDef {"Synchronize"};
ModeDefinition* UISynchronizeMode = &SynchronizeModeDef;

ModeDefinition ThresholdModeDef {"Threshold"};
ModeDefinition* UIThresholdMode = &ThresholdModeDef;


//
// Minor Modes
// 
// Mute and Overdub are both major and minor modes
//

ModeDefinition CaptureModeDef {"Capture"};
ModeDefinition* UICaptureMode = &CaptureModeDef;

ModeDefinition GlobalMuteModeDef {"GlobalMute"};
ModeDefinition* UIGlobalMuteMode = &GlobalMuteModeDef;

ModeDefinition GlobalPauseModeDef {"GlobalPause"};
ModeDefinition* UIGlobalPauseMode = &GlobalPauseModeDef;

ModeDefinition HalfSpeedModeDef {"HalfSpeed"};
ModeDefinition* UIHalfSpeedMode = &HalfSpeedModeDef;

ModeDefinition PitchOctaveModeDef {"PitchOctave"};
ModeDefinition* UIPitchOctaveMode = &PitchOctaveModeDef;

ModeDefinition PitchStepModeDef {"PitchStep"};
ModeDefinition* UIPitchStepMode = &PitchStepModeDef;

ModeDefinition PitchBendModeDef {"PitchBend"};
ModeDefinition* UIPitchBendMode = &PitchBendModeDef;

ModeDefinition SpeedOctaveModeDef {"SpeedOctave"};
ModeDefinition* UISpeedOctaveMode = &SpeedOctaveModeDef;

ModeDefinition SpeedStepModeDef {"SpeedStep"};
ModeDefinition* UISpeedStepMode = &SpeedStepModeDef;

ModeDefinition SpeedBendModeDef {"SpeedBend"};
ModeDefinition* UISpeedBendMode = &SpeedBendModeDef;

ModeDefinition SpeedToggleModeDef {"SpeedToggle"};
ModeDefinition* UISpeedToggleMode = &SpeedToggleModeDef;

ModeDefinition TimeStretchModeDef {"TimeStretch"};
ModeDefinition* UITimeStretchMode = &TimeStretchModeDef;

ModeDefinition ReverseModeDef {"Reverse"};
ModeDefinition* UIReverseMode = &ReverseModeDef;

ModeDefinition SoloModeDef {"Solo"};
ModeDefinition* UISoloMode = &SoloModeDef;

ModeDefinition WindowModeDef {"Window"};
ModeDefinition* UIWindowMode = &WindowModeDef;

