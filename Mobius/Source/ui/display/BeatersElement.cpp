/**
 * Status area component to display playback beats.
 *
 * BeaterElement manages a set of three Beaters to represent
 * reaching a subcycle, cycle, or loop boundary.  When a beater is turned on,
 * it will display highlighted for a period of time then turn off.
 *
 * To make these look responsive, the maintenance thread is broken out of its
 * wait state by a special engine callback mobiusTimeBoundary.
 * 
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../Supervisor.h"
#include "../MobiusView.h"

#include "Colors.h"
#include "StatusArea.h"
#include "BeatersElement.h"

/**
 * The number of milliseconds to keep a beater on.
 * This needs to be slow enough to be visible but fast enough
 * that adjacent beats don't smear together and keep the beater
 * lit all the time.
 *
 * We record the system millisecond counter when a beater is turned
 * on in the audio thread, and then let the maintenance thread
 * turn it off after this interval.
 */
const int BeaterDecayMsec = 100;

// experiment for cycle/loop to make them glow more
const int BeaterDecayLong = 200;

//////////////////////////////////////////////////////////////////////
//
// Beaters
//
//////////////////////////////////////////////////////////////////////

BeatersElement::BeatersElement(StatusArea* area) :
    StatusElement(area, "BeatersElement")
{
    addAndMakeVisible(&subcycleBeater);
    addAndMakeVisible(&cycleBeater);
    addAndMakeVisible(&loopBeater);

    // experiment with different decay times
    subcycleBeater.decayMsec = BeaterDecayMsec;
    cycleBeater.decayMsec = BeaterDecayLong;
    loopBeater.decayMsec = BeaterDecayLong;

    resizes = true;
}

BeatersElement::~BeatersElement()
{
}

/**
 * Old default diameter was 20.  We've got three of them.
 * Under Juce 20 feels smaller.
 */
const int BeaterDiameter = 30;
const int BeatersInset = 2;

int BeatersElement::getPreferredHeight()
{
    return BeaterDiameter + (BeatersInset * 2);;
}

int BeatersElement::getPreferredWidth()
{
    return (BeaterDiameter * 3) + (BeatersInset * 2);
}

void BeatersElement::resized()
{
    StatusElement::resized();
    
    // how much air should we leave around these?
    // might want to give them some padding

    juce::Rectangle<int> area = getLocalBounds();
    area = area.reduced(BeatersInset);
    subcycleBeater.setBounds(area.removeFromLeft(BeaterDiameter));
    cycleBeater.setBounds(area.removeFromLeft(BeaterDiameter));
    loopBeater.setBounds(area.removeFromLeft(BeaterDiameter));
}

/**
 * If we override paint, does that mean we control painting
 * the children, or is that going to cascade?
 */
void BeatersElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (!isIdentify()) {
        subcycleBeater.paintBeater(g);
        cycleBeater.paintBeater(g);
        loopBeater.paintBeater(g);
    }
}

/**
 * Maintenance thread refresh.
 * Notify the beaters that time has passed and repaint if
 * they've grown tired of life.
 *
 * For a time the three beat flags were "latching" meaning they
 * stayed on until the UI (us) turned them off.  This was from when we
 * tried to set them directly in the audio thread which caused probelms so
 * they should be able to be cleared by MobiusViewer like all the other
 * refresh flags.
 */
void BeatersElement::update(MobiusView* view)
{
    MobiusViewTrack* track = view->track;
    bool anyChanged = false;

    if (track->beatSubcycle) {
        if (subcycleBeater.start())
          anyChanged = true;
        track->beatSubcycle = false;
    }
    else if (subcycleBeater.tick())
      anyChanged = true;

    // beatCycle doesn't seem to be turning on for shorter loops?
    // weird...
    if (track->beatCycle) {
        if (cycleBeater.start())
          anyChanged = true;
        track->beatCycle = false;
    }
    else if (cycleBeater.tick())
      anyChanged = true;
    
    if (track->beatLoop) {
        if (loopBeater.start())
          anyChanged = true;

        // until we can figure out why beatCycle isn't always on
        // on loop boundaries, force it on
        if (cycleBeater.start())
          anyChanged = true;
        
        track->beatLoop = false;
    }
    else if (loopBeater.tick())
      anyChanged = true;
        
        
    if (anyChanged)
      repaint();
}

//////////////////////////////////////////////////////////////////////
//
// Beater
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by Beaters to turn us on.
 * Returns true if the graphics state changed.
 */
bool Beater::start()
{
    bool changed = false;
    if (on) {
        // we're already on, something is either wrong or
        // there are insanely fast subcycles
        // I tried extending the decay but it got smeary,
        // just ignore it, things would be happening so fast
        // you can't be watching beats for any useful timing
    } 
    else {
        // we're currently off
        startMsec = juce::Time::getMillisecondCounter();
        on = true;
        changed = true;
    }
    return changed;
}

/**
 * Called by Beaters every maintenance thread interval.
 * Return true if the light inside us dies.
 */
bool Beater::tick()
{
    bool changed = false;
    
    if (on) {
        int now = juce::Time::getMillisecondCounter();
        int delta = now - startMsec;
        if (delta > decayMsec) {
            on = false;
            changed = true;
        }
    }
    return changed;
}

bool Beater::reset()
{
    bool changed = false;
    if (on) {
        on = false;
        changed = true;
    }
    return changed;
}

/**
 * Called by Beaters to let our little light shine.
 * I'm assuming that Juce won't call paint on subcomponents
 * if the parent overrides paint?
 * todo: this really doesn't need to be a Component
 */
void Beater::paintBeater(juce::Graphics& g)
{
    // Ellipse wants float rectangles, getLocalBounds returns ints
    // seems like there should be an easier way to convert this
    juce::Rectangle<float> area ((float)getX(), (float)getY(), (float)getWidth(), (float)getHeight());
    
    // border
    g.setColour(juce::Colour(MobiusBlue));
    g.drawEllipse(area, 2.0f);
    
    if (on) {
        area = area.reduced(2.0f);
        g.setColour(juce::Colour(MobiusPink));
        g.fillEllipse(area);
    }
}

// forward mouse events to our parent
// since Beaters doesn't implement these, it ends up in StatusElement which
// handles the dragging

void Beater::mouseDown(const juce::MouseEvent& e)
{
    getParentComponent()->mouseDown(e);
}

void Beater::mouseDrag(const juce::MouseEvent& e)
{
    getParentComponent()->mouseDrag(e);
}

void Beater::mouseUp(const juce::MouseEvent& e)
{
    getParentComponent()->mouseUp(e);
}

void Beater::mouseEnter(const juce::MouseEvent& e)
{
    getParentComponent()->mouseEnter(e);
}

void Beater::mouseExit(const juce::MouseEvent& e)
{
    getParentComponent()->mouseExit(e);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
