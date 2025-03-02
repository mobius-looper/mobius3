/**
 * Base class for components within a TrackStrip
 */

#include <JuceHeader.h>

#include "../../util/Util.h"
#include "../MobiusView.h"

#include "Colors.h"
#include "TrackStrip.h"
#include "StripElement.h"

//////////////////////////////////////////////////////////////////////
//
// Definitions
//
// A set of static objects that define things about the elements
// that can be selected for display.  
//
//////////////////////////////////////////////////////////////////////

/**
 * Global registry
 */
std::vector<StripElementDefinition*> StripElementDefinition::Elements;

/**
 * Find a strip element definition by name
 */
StripElementDefinition* StripElementDefinition::find(const char* name)
{
	StripElementDefinition* found = nullptr;
	
	for (int i = 0 ; i < Elements.size() ; i++) {
		StripElementDefinition* d = Elements[i];
		if (StringEqualNoCase(d->getName(), name)) {
            found = d;
            break;
        }
	}
	return found;
}

/**
 * Name is how we refer to them internally in the UIConfig
 * Display name is what we show in the UI
 *
 * !! hating how this works
 * we've got static name dependencies now that UIParameter is gone
 * should be able to build these from ParameterProperties but then they
 * can't be static which I also hate
 */
StripElementDefinition::StripElementDefinition(const char* argName, const char* argDisplayName)
{
    name = argName;
    displayName = argDisplayName;
    Elements.push_back(this);
}

const char* StripElementDefinition::getName()
{
    return name;
}

const char* StripElementDefinition::getDisplayName()
{
    return displayName;
}

const char* StripElementDefinition::getDisplayableName()
{
    return (displayName != nullptr) ? displayName : name;
}

// I'm really growing to hate this little dance, find a better way!

StripElementDefinition StripInputObj {"input", "Input"};
StripElementDefinition* StripDefinitionInput = &StripInputObj;

StripElementDefinition StripOutputObj {"output", "Output"};
StripElementDefinition* StripDefinitionOutput = &StripOutputObj;

StripElementDefinition StripFeedbackObj {"feedback", "Feedback"};
StripElementDefinition* StripDefinitionFeedback = &StripFeedbackObj;

StripElementDefinition StripAltFeedbackObj {"altFeedback", "Alt Feedback"};
StripElementDefinition* StripDefinitionAltFeedback = &StripAltFeedbackObj;

StripElementDefinition StripPanObj {"pan", "Pan"};
StripElementDefinition* StripDefinitionPan = &StripPanObj;

// the defaults for the dock, also OutputLevel
StripElementDefinition StripTrackNumberObj {"trackNumber", "Track Number"};
StripElementDefinition* StripDefinitionTrackNumber = &StripTrackNumberObj;

StripElementDefinition StripMasterObj {"masters", "Masters"};
StripElementDefinition* StripDefinitionMaster = &StripMasterObj;

StripElementDefinition StripLoopRadarObj {"loopRadar", "Loop Radar"};
StripElementDefinition* StripDefinitionLoopRadar = &StripLoopRadarObj;

// formerly called "loopStatus"
StripElementDefinition StripLoopStackObj {"loopStack", "Loop Stack"};
StripElementDefinition* StripDefinitionLoopStack = &StripLoopStackObj;

StripElementDefinition StripOutputMeterObj {"outputMeter", "Output Meter"};
StripElementDefinition* StripDefinitionOutputMeter = &StripOutputMeterObj;
StripElementDefinition StripInputMeterObj {"inputMeter", "Input Meter"};
StripElementDefinition* StripDefinitionInputMeter = &StripInputMeterObj;


// optional but popular
StripElementDefinition StripGroupNameObj {"groupName", "Group Name"};
StripElementDefinition* StripDefinitionGroupName = &StripGroupNameObj;

StripElementDefinition StripLoopThermometerObj {"loopMeter", "Loop Meter"};
StripElementDefinition* StripDefinitionLoopThermometer = &StripLoopThermometerObj;

// obscure options

// this was a little button we don't need if the track
// number is clickable for focus
StripElementDefinition StripFocusLockObj {"focusLock", "Focus Lock"};
StripElementDefinition* StripDefinitionFocusLock = &StripFocusLockObj;

StripElementDefinition StripPitchOctaveObj {"pitchOctave", "Pitch Octave"};
StripElementDefinition* StripDefinitionPitchOctave = &StripPitchOctaveObj;

StripElementDefinition StripPitchStepObj {"pitchStep", "Pitch Step"};
StripElementDefinition* StripDefinitionPitchStep = &StripPitchStepObj;

StripElementDefinition StripPitchBendObj {"pitchBend", "Pitch Bend"};
StripElementDefinition* StripDefinitionPitchBend = &StripPitchBendObj;

StripElementDefinition StripSpeedOctaveObj {"speedOctave", "Speed Octave"};
StripElementDefinition* StripDefinitionSpeedOctave = &StripSpeedOctaveObj;

StripElementDefinition StripSpeedStepObj {"speedStep", "Speed Step"};
StripElementDefinition* StripDefinitionSpeedStep = &StripSpeedStepObj;

StripElementDefinition StripSpeedBendObj {"speedBend", "Speed Bend"};
StripElementDefinition* StripDefinitionSpeedBend = &StripSpeedBendObj;

StripElementDefinition StripTimeStretchObj {"timeStretch", "Time Stretch"};
StripElementDefinition* StripDefinitionTimeStretch = &StripTimeStretchObj;

// find a way to put these inside StripElement for namespace

const StripElementDefinition* StripDockDefaults[] = {
    StripDefinitionTrackNumber,
    StripDefinitionMaster,
    StripDefinitionLoopRadar,
    //StripDefinitionLoopThermometer,
    StripDefinitionLoopStack,
    StripDefinitionOutput,
    StripDefinitionOutputMeter,
    nullptr
};

//////////////////////////////////////////////////////////////////////
//
// Component
//
//////////////////////////////////////////////////////////////////////

/**
 * Element that doesn't have a Parameter
 * Put the definition name in ComponentID for searching
 * with Juce::Component::findChildWithComponentID
 * Put the name in Component::name for JuceUtil trace
 */
StripElement::StripElement(TrackStrip* parent, StripElementDefinition* def)
{
    strip = parent;
    // this is now optional for UIElementStripAdapter that doesn't need one
    definition = def;
    if (def != nullptr) {
        setComponentID(def->getName());
        setName(def->getName());
    }
}
        
StripElement::~StripElement()
{
}

MobiusView* StripElement::getMobiusView()
{
    return strip->getMobiusView();
}

MobiusViewTrack* StripElement::getTrackView()
{
    return strip->getTrackView();
}

void StripElement::update(MobiusView* view)
{
    (void)view;
}

// these should probably be pure virtual
// any useful thing to do in a default implementation?

int StripElement::getPreferredWidth()
{
    return 50;
}

int StripElement::getPreferredHeight()
{
    return 20;
}

/**
 * todo: might want some default painting for borders, labels,
 * and size dragging.  Either the subclass must call back up to this
 * or we have a different paintElement function.
 */
void StripElement::paint(juce::Graphics& g)
{
    (void)g;
}

/**
 * Allow StripElements to activate the track they are in when clicked.
 * This forwards up to TrackStrip which handles it when you click
 * outside the boudns of an element.
 * 
 * The ones with sub-components like StripRotary won't support this
 * since Juce goes bottom-up handling mouse events.  But at lest
 * the track number will work.
 *
 * todo: will want subclass specific click beyavior.
 * TrackNumber used to also control focus, and LoopStack could switch loops.
 */
void StripElement::mouseDown(const juce::MouseEvent& event)
{
    if (strip->isDocked())
      strip->mouseDown(event);
}

void StripElement::mouseEnter(const juce::MouseEvent& e)
{
    (void)e;
    mouseEntered = true;
    // what StatusElement does
    //repaint();
}

void StripElement::mouseExit(const juce::MouseEvent& e)
{
    (void)e;
    mouseEntered = false;
    //repaint();
}
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
