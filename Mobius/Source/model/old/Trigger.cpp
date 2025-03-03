/**
 * A collection of static objects that define the types of things
 * that can cause something to happen.  They are part of both
 * the Action and Binding models, but factored out because they
 * need to be used are used at various levels that don't need to
 * understand where they came from.
 *
 * They self initialize during static initialization and will
 * self destruct.
 *
 */

#include "Trigger.h"

//////////////////////////////////////////////////////////////////////
//
// Triggers
//
//////////////////////////////////////////////////////////////////////

/**
 * Do we really need display names for these?
 * We don't currently show a consolidated table of all
 * merged bindings and even if we did the internal name is enough.
 */
Trigger::Trigger(const char* name, const char* display) :
    SystemConstant(name, display)
{
    Instances.push_back(this);
}

/**
 * This formerly tested a "bindable" flag and filtered
 * those out.  Unbindables were things like TriggerEvent
 * and TriggerThread which I don't think we need.
 */
Trigger* Trigger::find(const char* name) 
{
	Trigger* found = nullptr;
	if (name != nullptr) {
		for (int i = 0 ; Instances.size() ; i++) {
			Trigger* t = Instances[i];
			if (!strcmp(t->getName(), name)) {
				found = t;
				break;
			}
		}
	}
	return found;
}

/**
 * Until we decide to stop using concrete MIDI event types
 * for trigger types, provide a convenient type tester.
 */
bool Trigger::isMidi(Trigger* t)
{
    return (t == TriggerMidi ||
            t == TriggerNote ||
            t == TriggerProgram ||
            t == TriggerControl ||
            t == TriggerPitch);
}

std::vector<Trigger*> Trigger::Instances;

// unlike Parameter we don't have subclasses so can just extern
// the Trigger object
// everything really wants to deal with a pointer to them and I don't want
// to mess with reference conversion right now

Trigger TriggerKeyObj("key", "Key");
Trigger* TriggerKey = &TriggerKeyObj;

Trigger TriggerMidiObj("midi", "MIDI");
Trigger* TriggerMidi = &TriggerMidiObj;

Trigger TriggerHostObj("host", "Host");
Trigger* TriggerHost = &TriggerHostObj;

Trigger TriggerOscObj("osc", "OSC");
Trigger* TriggerOsc = &TriggerOscObj;

Trigger TriggerUIObj("ui", "UI");
Trigger* TriggerUI = &TriggerUIObj;

// have been using these in Bindings to make it
// easier to identify the most common trigger types
// rather than just trigger="midi" 
// think about converting this to just trigger='midi' with midiType='note'

Trigger TriggerNoteObj("note", "Note");
Trigger* TriggerNote = &TriggerNoteObj;

Trigger TriggerProgramObj("program", "Program");
Trigger* TriggerProgram = &TriggerProgramObj;

Trigger TriggerControlObj("control", "Control");
Trigger* TriggerControl = &TriggerControlObj;

Trigger TriggerPitchObj("pitch", "Pitch Bend");
Trigger* TriggerPitch = &TriggerPitchObj;

// these were special-case triggers that may not be necessary
// revisit with the engine porting is complete

Trigger TriggerScriptObj("script", "Script");
Trigger* TriggerScript = &TriggerScriptObj;

Trigger TriggerAlertObj("alert", "Alert");
Trigger* TriggerAlert = &TriggerAlertObj;

Trigger TriggerEventObj("event", "Event");
Trigger* TriggerEvent = &TriggerEventObj;

Trigger TriggerThreadObj("thread", "Mobius Thread");
Trigger* TriggerThread = &TriggerThreadObj;

Trigger TriggerUnknownObj("unknown", "unknown");
Trigger* TriggerUnknown = &TriggerUnknownObj;

//////////////////////////////////////////////////////////////////////
//
// Trigger Modes
//
// These were part of the old model and not currently used in the UI.
// It seems useful though so keep it around.
//
//////////////////////////////////////////////////////////////////////

std::vector<TriggerMode*> TriggerMode::Instances;

TriggerMode TriggerModeOnceObj("once", "Once");
TriggerMode* TriggerModeOnce = &TriggerModeOnceObj;

TriggerMode TriggerModeMomentaryObj("momentary", "Momentary");
TriggerMode* TriggerModeMomentary = &TriggerModeMomentaryObj;

TriggerMode TriggerModeContinuousObj("continuous", "Continuous");
TriggerMode* TriggerModeContinuous = &TriggerModeContinuousObj;

TriggerMode TriggerModeToggleObj("toggle", "Toggle");
TriggerMode* TriggerModeToggle = &TriggerModeToggleObj;

TriggerMode TriggerModeXYObj("xy", "X,Y");
TriggerMode* TriggerModeXY = &TriggerModeXYObj;


TriggerMode::TriggerMode(const char* name, const char* display) :
    SystemConstant(name, display)
{
    Instances.push_back(this);
}

TriggerMode* TriggerMode::find(const char* name) 
{
	TriggerMode* found = nullptr;
	if (name != nullptr) {
		for (int i = 0 ; i < Instances.size() ; i++) {
			TriggerMode* t = Instances[i];
			if (!strcmp(t->getName(), name)) {
				found = t;
				break;
			}
		}
	}
	return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
