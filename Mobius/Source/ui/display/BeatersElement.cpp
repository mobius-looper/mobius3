/**
 * Status area component to display playback beats.
 *
 * BeaterElement manages a set of three Beaters to represent
 * reaching a subcycle, cycle, or loop boundary.  When a beater is turned on,
 * it will display highlighted for a period of time then turn off.
 *
 * Originally used maintenance thread intervals to turn these on and off
 * but with a 1/10th cycle it's just too jittery to look right.
 *
 * We're taking the unusual step of letting the audio thread
 * call MobiusInterface::mobiusTimeBoundary directly which
 * ends up in Supervisor::TimeListener::timeBoundary below.
 *
 * The only thing we're allowed to do is check beater state
 * and call repaint() to ask the UI thread to refresh.
 *
 * The maintenance thread will still be responsible for turning
 * beaters off, which will still have jitter but doesn't look
 * as bad as not seeing them turn on.
 * 
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/MobiusState.h"
#include "../../model/ModeDefinition.h"
#include "../../Supervisor.h"

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

    // receive notifications of time boudaries closer to when they happen
    Supervisor::Instance->addTimeListener(this);
}

BeatersElement::~BeatersElement()
{
    Supervisor::Instance->removeTimeListener(this);
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
 * Newer interface to receive notifications from
 * the engine via MobiusListener::mobiusTimeBoundary
 * close to when a subcycle/cycle/loop boundary was crossed.
 *
 * NOTE WELL: This is called from the AUDIO THREAD so the only
 * thing you can do is twiddle memory and call repaint().
 * See MobiusKernel::coreTimeBoundary for more commentary.
 *
 * Capture the latched state of the beats from MobiusState
 * and reset the latch.  See file comments for why this
 * has to be a latch.
 *
 * !! I've noticed the cycle beater not turning on with short loops
 * and more rarely loop beater.  This seemed to happen after
 * the conversion to using MobiusTimeBoundary exclusively to
 * turn things on.  So Mobius logic to set the beat flags may
 * have a roundoff error.
 */
void BeatersElement::timeBoundary(MobiusState* state)
{
    MobiusTrackState* track = &(state->tracks[state->activeTrack]);
    MobiusLoopState* loop = &(track->loops[track->activeLoop]);

    bool anyChanged = false;

    if (loop->beatSubCycle) {
        if (subcycleBeater.start())
          anyChanged = true;
        loop->beatSubCycle = false;
    }

    // beatCycle doesn't seem to be turning on for shorter loops
    // weird...
    if (loop->beatCycle) {
        if (cycleBeater.start())
          anyChanged = true;
        loop->beatCycle = false;
    }
    
    if (loop->beatLoop) {
        if (loopBeater.start())
          anyChanged = true;

        // until we can figure out why beatCycle isn't always on
        // on loop boundaries, force it on
        if (cycleBeater.start())
          anyChanged = true;
        
        loop->beatLoop = false;
    }
        
    if (anyChanged)
      repaint();
}

/**
 * Maintenance thread refresh.
 * Notify the beaters that time has passed and repaint if
 * they've grown tired of life.
 */
void BeatersElement::update(MobiusState* state)
{
    (void)state;

    bool anyChanged = false;

    // detect missed timeBoundaries, this shouldn't be necessary
    // and means that Mobius must not be setting the flags
    // correctly under some buffer conditions
    // since both the audio thread and maintenance thread are in play
    // here there are race conditions on the state flags so don't clear them
    MobiusTrackState* track = &(state->tracks[state->activeTrack]);
    MobiusLoopState* loop = &(track->loops[track->activeLoop]);

    // in brief testing, I did hit missed loop but not cycle
    if (loop->beatCycle && !cycleBeater.on) {
        Trace(2, "BeatersElement: missed a cycle\n");
    }
    if (loop->beatLoop && !loopBeater.on) {
        Trace(2, "BeatersElement: missed a loop\n");
    }

    if (subcycleBeater.tick())
      anyChanged = true;
    
    if (cycleBeater.tick())
      anyChanged = true;

    if (loopBeater.tick())
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
        startMsec = Supervisor::Instance->getMillisecondCounter();
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
        int now = Supervisor::Instance->getMillisecondCounter();
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
