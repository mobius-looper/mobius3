#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/MobiusState.h"
#include "../../model/UIParameter.h"
#include "../../model/MobiusConfig.h"
#include "../../model/UIConfig.h"

#include "Colors.h"
#include "TrackStrip.h"
#include "StripElement.h"
#include "StripElements.h"

//////////////////////////////////////////////////////////////////////
//
// TrackNumber
//
//////////////////////////////////////////////////////////////////////

// This formerly also functioned as the focus lock widget
// Might be nice to have that by clicking on it rather than making
// them take up space with the FocusLock button, but clicking on
// the number is also a very common way to select tracks with the mouse
// so not sure...

// number vs. name
// Old code displayed either the number or the track name if one was
// set in the Setup.  Since names are variable, the preferred size
// needs to be reasonably wide.

StripTrackNumber::StripTrackNumber(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionTrackNumber)
{
    action.symbol = strip->getSupervisor()->getSymbols()->intern("FocusLock");
    action.scopeTrack = parent->getTrackIndex() + 1;
}

StripTrackNumber::~StripTrackNumber()
{
}

int StripTrackNumber::getPreferredWidth()
{
    // this is what we started with when just displaying a number
    // make it wider for names
    //return 60;
    return 180;
}

int StripTrackNumber::getPreferredHeight()
{
    return 30;
}

/**
 * In theory they do an action to change the track name
 * and make it different than the Setup.  There isn't a good
 * place to pull that right now, not even sure if Query works.
 * But startingSetup will be enough for most.
 */
void StripTrackNumber::configure()
{
    // could refresh the name here, but just reset the setup ordinal
    // so we pick it up on the next update
    setupOrdinal = -1;
}

void StripTrackNumber::update(MobiusState* state)
{
    int tracknum = strip->getTrackIndex();
    MobiusTrackState* track = &(state->tracks[tracknum]);

    bool needsRepaint = false;

    // whenever the setup changes, refresh the name
    if (setupOrdinal != state->setupOrdinal) {
        trackName = "";
        MobiusConfig* config = strip->getSupervisor()->getMobiusConfig();
        Setup* setup = config->getSetup(state->setupOrdinal);
        if (setup != nullptr) {
            SetupTrack* st = setup->getTrack(tracknum);
            if (st != nullptr)
              trackName = juce::String(st->getName());
        }
        setupOrdinal = state->setupOrdinal;
        needsRepaint = true;
    }
    
    if (track->focusLock != focusLock) {
        focusLock = track->focusLock;
        needsRepaint = true;
    }

    if (needsRepaint)
      repaint();
}

/**
 * drawFittedText
 * arg after justification is maximumNumberOfLines
 * which can be used to break up the next into several lines
 * last arg is minimumHorizontalScale which specifies
 * "how much the text can be squashed horizontally"
 * set this to 1.0f to prevent horizontal squashing
 * what that does is unclear, leaving it zero didn't seem to adjust
 * the font height, 0.5 was smaller but still not enough for
 * "This is a really long name"
 * adding another line didn't do anything, maybe the area needs
 * to be a multiple of the font height?
 * What I was expecting is that it would scale the font height down
 * to make everything smaller, but it doesn't seem to do that
 * setting this to 1.0f had the effect I expected, the font got smaller
 * and it used multiple lines.  So if this isn't 1.0 it prefers
 * squashing over font manipulation and mulitple lines.
 * "This is a name longer than anyone should use" mostly displayed
 * it lots the first "T" and the "n" at the end of "than" on the first
 * line, and the second line was complete and centered.  But
 * smaller names are okay, and I've spent enough time trying to figure
 * out exactly how this works.
 *
 * Still getting some truncation on the left and right for text that
 * fits mostly on one line "This is a long" will lose the left half
 * of the initial T and half of the final g.  Don't know if this is
 * an artifact of drawFittedText, or if I have bounds messed up somewhere.
 */
void StripTrackNumber::paint(juce::Graphics& g)
{
    juce::Colour textColor = juce::Colour(MobiusGreen);
    if (focusLock)
      textColor = juce::Colour(MobiusRed);

    g.setColour(textColor);
    
    if (trackName.length() == 0) {
        juce::Font font((float)getHeight());

        g.setFont(font);

        // if we're docked, the TrackStrip has the number
        // otherwise update must have remembered the active track

        g.drawText(juce::String(strip->getTrackIndex() + 1), 0, 0, getWidth(), getHeight(),
                   juce::Justification::centred);
    }
    else {
        juce::Font font((float)getHeight());
        // hacking around the unpredictable truncation, if the name is beyond
        // a certain length, reduce the font height
        if (trackName.length() >= 10)
          font = juce::Font((float)(getHeight() * 0.75f));
          
        // not sure about font sizes, we're going to use fit so I think
        // that will size down as necessary

        g.setFont(font);
        g.drawFittedText(trackName, 0, 0, getWidth(), getHeight(),
                         juce::Justification::centred,
                         1, // max lines
                         1.0f);
    }
}

/**
 * Like focus lock, this one has to deal both with making the current track
 * active and toggling focus.
 *
 * Since clicking over the name is extremely common when selecting tracks,
 * handle this in phases.  If the track is not active, just activate it without
 * changing focus.
 */
void StripTrackNumber::mouseDown(const class juce::MouseEvent& event)
{
    if (!strip->isActive()) {
        // superclass will activate it
        StripElement::mouseDown(event);
    }
    else {
        strip->getSupervisor()->doAction(&action);
    }
}

//////////////////////////////////////////////////////////////////////
//
// FocusLock
//
//////////////////////////////////////////////////////////////////////

StripFocusLock::StripFocusLock(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionFocusLock)
{
    action.symbol = strip->getSupervisor()->getSymbols()->intern("FocusLock");
    // TrackStrip track numbers are zero based, should call
    // this TrackIndex!
    action.scopeTrack = parent->getTrackIndex() + 1;
}

StripFocusLock::~StripFocusLock()
{
}

int StripFocusLock::getPreferredWidth()
{
    return 14;
}

int StripFocusLock::getPreferredHeight()
{
    return 14;
}

void StripFocusLock::update(MobiusState* state)
{
    int tracknum = strip->getTrackIndex();
    MobiusTrackState* track = &(state->tracks[tracknum]);

    if (track->focusLock != focusLock) {
        focusLock = track->focusLock;
        repaint();
    }
}

void StripFocusLock::paint(juce::Graphics& g)
{
    // Ellipse wants float rectangles, getLocalBounds returns ints
    // seems like there should be an easier way to convert this
    juce::Rectangle<float> area (0.0f, 0.0f, (float)getWidth(), (float)getHeight());

    // clips a little
    area = area.reduced(2.0f);
    
    g.setColour(juce::Colours::white);
    g.drawEllipse(area, 2.0f);

    if (focusLock) {
        g.setColour(juce::Colour(MobiusRed));
        area = area.reduced(2.0f);
        g.fillEllipse(area);
    }
}

/**
 * This one's a little weird because we potentialy do two things.
 * 
 * StripElement::mouseDown will generate an action to select the track
 * if it isn't currently selected.
 * Here, we send an action to toggle focus lock.
 * Unclear what the ordering will be or if it matters, both will end
 * on the Kernel action list at the same time.
 */
void StripFocusLock::mouseDown(const class juce::MouseEvent& event)
{
    // select the track first?
    StripElement::mouseDown(event);
    
    strip->getSupervisor()->doAction(&action);
}

//////////////////////////////////////////////////////////////////////
//
// LoopRadar
//
//////////////////////////////////////////////////////////////////////

const int LoopRadarDiameter = 30;
const int LoopRadarPadding = 4;

StripLoopRadar::StripLoopRadar(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionLoopRadar)
{
}

StripLoopRadar::~StripLoopRadar()
{
}

int StripLoopRadar::getPreferredWidth()
{
    
    return LoopRadarDiameter + (LoopRadarPadding * 2);
}

int StripLoopRadar::getPreferredHeight()
{
    return LoopRadarDiameter + (LoopRadarPadding * 2);
}

void StripLoopRadar::update(MobiusState* state)
{
    int tracknum = strip->getTrackIndex();
    MobiusTrackState* track = &(state->tracks[tracknum]);
    MobiusLoopState* loop = &(track->loops[track->activeLoop]);

    juce::Colour color = Colors::getLoopColor(loop);

    if (loop->frame != loopFrame ||
        loop->frames != loopFrames ||
        color != loopColor) {
        loopFrame = loop->frame;
        loopFrames = loop->frames;
        loopColor = color;
        repaint();
    }
}

/**
 * Radians docs:
 * "the angle (clockwise) in radians at which to start the arc segment where
 * zero is the top center of the ellipse"
 *
 * "The radius is the distance from the center of a circle to its perimeter.
 * A radian is an angle whose corresponding arc in a circle is equal to the
 * radius of the circle"
 *
 * pi radians is 180 degrees so a full filled circle is 2pi
 *
 * For radians proportional to the position within a loop first get the loop
 * position as a fraction of the total loop:
 *
 *   float loopFraction = loopFrames / loopFrame;
 *
 * Then multiply that by 2pi
 *    
 */
void StripLoopRadar::paint(juce::Graphics& g)
{
    float twopi = 6.28318f;

    // StripElement::paint(g);

    // start by redrawing the pie every time, can get smarter later
    g.setColour(juce::Colours::black);
    g.fillRect(0.0f, 0.0f, (float)getWidth(), (float)getHeight());

    if (loopFrames > 0) {
        float frames = (float)loopFrames;
        float frame = (float)loopFrame;
        float fraction = frame / frames;
        float startrad = 0.0f;
        float endrad = twopi * fraction;

        juce::Path path;
        int innerCircle = 0;

        // start radians, end radians, inner circle 
        path.addPieSegment((float)LoopRadarPadding, (float)LoopRadarPadding,
                           (float)LoopRadarDiameter, (float)LoopRadarDiameter,
                           startrad, endrad, (float)innerCircle);

        g.setColour(loopColor);
        g.fillPath(path);
    }
    else {
        // color shold have been left red if recording
        g.setColour(loopColor);
        g.fillEllipse((float)LoopRadarPadding, (float)LoopRadarPadding,
                      (float)LoopRadarDiameter, (float)LoopRadarDiameter);
    }
}

//////////////////////////////////////////////////////////////////////
//
// LoopThermometer
//
// Alternative to radar that takes up less vertical space but more horizontal
//
//////////////////////////////////////////////////////////////////////

StripLoopThermometer::StripLoopThermometer(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionLoopThermometer)
{
}

StripLoopThermometer::~StripLoopThermometer()
{
}

int StripLoopThermometer::getPreferredWidth()
{
    return 100;
}

int StripLoopThermometer::getPreferredHeight()
{
    return 10;
}

void StripLoopThermometer::update(MobiusState* state)
{
    int tracknum = strip->getTrackIndex();
    MobiusTrackState* track = &(state->tracks[tracknum]);
    MobiusLoopState* loop = &(track->loops[track->activeLoop]);

    if (loop->frame != loopFrame ||
        loop->frames != loopFrames) {

        loopFrame = loop->frame;
        loopFrames = loop->frames;
        repaint();
    }
}

void StripLoopThermometer::paint(juce::Graphics& g)
{
    // StripElement::paint(g);

    // start by redrawing the pie every time, can get smarter later
    g.setColour(juce::Colours::black);
    g.fillRect(0.0f, 0.0f, (float)getWidth(), (float)getHeight());

    if (loopFrames > 0) {
        float frames = (float)loopFrames;
        float frame = (float)loopFrame;
        float fraction = frame / frames;
        float width = (float)getWidth() * fraction;
        
        g.setColour(juce::Colour(MobiusRed));
        g.fillRect(0.0f, 0.0f, width, (float)getHeight());
    }
}

//////////////////////////////////////////////////////////////////////
//
// OutputMeter
//
//////////////////////////////////////////////////////////////////////

StripOutputMeter::StripOutputMeter(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionOutputMeter)
{
    addAndMakeVisible(&meter);
}

StripOutputMeter::~StripOutputMeter()
{
}

int StripOutputMeter::getPreferredWidth()
{
    return 100;
}

int StripOutputMeter::getPreferredHeight()
{
    return 10;
}

void StripOutputMeter::update(MobiusState* state)
{
    int tracknum = strip->getTrackIndex();
    MobiusTrackState* track = &(state->tracks[tracknum]);

    meter.update(track->outputMonitorLevel);
}

void StripOutputMeter::resized()
{
    meter.setBounds(getLocalBounds());
}

//////////////////////////////////////////////////////////////////////
//
// InputMeter
//
//////////////////////////////////////////////////////////////////////

StripInputMeter::StripInputMeter(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionInputMeter)
{
    addAndMakeVisible(&meter);
}

StripInputMeter::~StripInputMeter()
{
}

int StripInputMeter::getPreferredWidth()
{
    return 100;
}

int StripInputMeter::getPreferredHeight()
{
    return 10;
}

void StripInputMeter::update(MobiusState* state)
{
    int tracknum = strip->getTrackIndex();
    MobiusTrackState* track = &(state->tracks[tracknum]);

    meter.update(track->inputMonitorLevel);
}

void StripInputMeter::resized()
{
    meter.setBounds(getLocalBounds());
}

//////////////////////////////////////////////////////////////////////
//
// LoopStack
//
// Displays brief information about all loops in a track
//
// Well it's actually not "all" only the first 4.  Originally thought about
// making this sensitive to MobiusConfig::loopCount and resizing if that
// was edited.  But I think I'd rather have this be a fixed number, and
// then "scroll" among the loops actually used by the track.
//
//////////////////////////////////////////////////////////////////////

const int LoopStackRowHeight = 12;
const int LoopStackNumberWidth = 12;
const int LoopStackHorizontalGap = 10;
const int LoopStackVerticalGap = 1;
const int LoopStackRectangleWidth = 60;
const int LoopStackBorderWidth = 1;
const int LoopStackDefaultLoopRows = 4;

StripLoopStack::StripLoopStack(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionLoopStack)
{
    maxLoops = LoopStackDefaultLoopRows;
}

StripLoopStack::~StripLoopStack()
{
}

void StripLoopStack::configure()
{
    // todo, here is where we should allow the maximum loop display to be set
    UIConfig* config = strip->getSupervisor()->getUIConfig();
    int max = config->getInt("loopRows");
    // no need to get extreme on this if they type in something wrong
    if (max > 0 && max <= 16)
      maxLoops = max;
}

int StripLoopStack::getPreferredWidth()
{
    return LoopStackNumberWidth + LoopStackHorizontalGap + LoopStackRectangleWidth;
}

/**
 * todo: to prevent this from becomming excessively large, should support
 * a maximum number of displayable loops and scroll within that.
 */
int StripLoopStack::getPreferredHeight()
{
    return (LoopStackRowHeight + LoopStackVerticalGap) * maxLoops;
}

/**
 * Like LoopMeter, we've got a more complex than usual substructure
 * so it is harder to do difference detection.
 * Update whenever the loop is moving for now.  This also gives us
 * a way to do progress bars if desired.
 *
 * This is the only thing right now that uses the kludgey needsRefresh
 * flag from MobiusTrackState which will have been saved in the
 * parent strip.  Used to detect when a file drop finishes since
 * we're not very thorough doing change detection on non-active tracks.
 */
void StripLoopStack::update(MobiusState* state)
{
    int tracknum = strip->getTrackIndex();

    bool needsRefresh = (strip != nullptr && strip->needsRefresh);
    
    // paint needs the entire track so save it locally
    track = &(state->tracks[tracknum]);
    MobiusLoopState* activeLoop = &(track->loops[track->activeLoop]);

    if (needsRefresh ||
        trackLoops != track->loopCount ||
        lastActive != track->activeLoop ||
        activeLoop->frame != lastFrame ||
        dropTarget != lastDropTarget) {

        trackLoops = track->loopCount;
        lastActive = track->activeLoop;
        lastFrame = activeLoop->frame;
        lastDropTarget = dropTarget;
        repaint();
    }
}

/**
 * Display a row for each loop with a filed rectangle representing loop state.
 * Old code was pretty basic, we could do a lot more now.
 */
void StripLoopStack::paint(juce::Graphics& g)
{
    // don't paint until an update() has happened and we've saved
    // the TrackState we're supposed to be dealing with
    // must have saved this in update
    if (track == nullptr) {
        return;
    }

    // determine the origin of the loops to display
    // normally this is zero, but can be larger if the track
    // has more loops than the maximum we display
    // there are a number of ways we could orient this, might
    // be nice to keep at most one loop under the active one
    // for now, just make sure it's visible at the bottom
    int origin = 0;
    int active = track->activeLoop;
    if (active >= maxLoops) {
        origin = (active - maxLoops) + 1;
    }
    firstLoop = origin;

    // to properly color the switch destination, we have to first find
    // the active loop and look there
    // although it is really a property of an event, summaraizer puts
    // it here to make it easier to find, should really be on the track
    // not sure why we made a distinction between switch and return, I guess
    // there was a little "R" somewhere, here we don't care
    // nextLoop and returnLoop are 1 based
    int switchDestination = -1;
    MobiusLoopState* loop = &(track->loops[track->activeLoop]);
    if (loop->nextLoop >= 0)
      switchDestination = loop->nextLoop - 1;
    else if (loop->returnLoop >= 0)
      switchDestination = loop->returnLoop - 1;
    
    for (int row = 0 ; row < maxLoops ; row++) {

        int loopIndex = origin + row;
        if (loopIndex >= trackLoops) {
            // nothing more to display
            break;
        }
            
        loop = &(track->loops[loopIndex]);

        int rowTop = (LoopStackRowHeight + LoopStackVerticalGap) * row;
        
        // loop number
        if (loopIndex == track->activeLoop)
          g.setColour(juce::Colours::white);
        else
          g.setColour(juce::Colours::green);    // was a darker green

        g.drawText(juce::String(loopIndex+1), 0, rowTop, LoopStackNumberWidth, LoopStackRowHeight,
                   juce::Justification::centred);
        
        // border: white=active, black=inactive, yellow=switching, red=switchDestination
        // if we're recording and switching yellow may not stand out enough?

        // the drop target shares the same color as switch destionation which
        // isn't too bad, but might want to make drop target a bit more extreme
        if (loopIndex == dropTarget) {
            g.setColour(juce::Colours::red);
        }
        else if (loopIndex == track->activeLoop) {
            // It's possible to switch to the same loop, an alternate
            // way to stack events, need a third color for this?
            if (switchDestination >= 0)
              g.setColour(juce::Colours::yellow);
            else
              g.setColour(juce::Colours::white);
        }
        else if (loopIndex == switchDestination) {
            g.setColour(juce::Colours::red);
        }
        else {
            // empty, leave it black, or just don't draw it
            g.setColour(juce::Colours::black);
        }

        int rectLeft = LoopStackNumberWidth + LoopStackHorizontalGap;
        // adjust for available size or keep it fixed?
        int rectWidth = getWidth() - rectLeft;
        g.drawRect(rectLeft, rowTop, rectWidth, LoopStackRowHeight);

        // border inset
        int blockLeft = rectLeft + LoopStackBorderWidth;
        int blockTop = rowTop + LoopStackBorderWidth;
        int blockWidth = rectWidth - (LoopStackBorderWidth * 2);
        int blockHeight = LoopStackRowHeight - (LoopStackBorderWidth * 2);

        // block: black=empty, grey=full, green=play, red=record, blue=mute
        // old code used grey to mean 1/2 speed

        if (loop->frames > 0) {
            juce::Colour color;
            if (loopIndex != track->activeLoop) {
                // OG mobius always drew this green, since green usually
                // means "playing" not sure I like that
                // since grey is used for half speed, darken it
                color = juce::Colours::darkgrey;
            }
            else {
                color = Colors::getLoopColor(loop);
            }
            
            g.setColour(color);

            g.fillRect(blockLeft, blockTop, blockWidth, blockHeight);
        }
        // else empty, leave it black
    }
}

bool StripLoopStack::isInterestedInFileDrag(const juce::StringArray& files)
{
    (void)files;
    return true;
}

/**
 * Calulate which loop row the mouse is over.  The stack occupies the entire
 * height, so there will always be something.
 */
int StripLoopStack::getDropTarget(int x, int y)
{
    (void)x;
    int rowHeight = LoopStackRowHeight + LoopStackVerticalGap;
    int target = y / rowHeight;

    // not so fast!  now that we have a scrolling display, have to factor in the origin
    target += firstLoop;
    
    return target;
}    

void StripLoopStack::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    (void)files;
    //Trace(2, "StripLoopStack: fileDragEnter\n");
    dropTarget = getDropTarget(x, y);
}

void StripLoopStack::fileDragMove(const juce::StringArray& files, int x, int y)
{
    (void)files;
    //Trace(2, "StripLoopStack: fileDragMove\n");
    dropTarget = getDropTarget(x, y);
}

void StripLoopStack::fileDragExit(const juce::StringArray& files)
{
    (void)files;
    //Trace(2, "StripLoopStack: fileDragExit\n");
    dropTarget = -1;
}

void StripLoopStack::filesDropped(const juce::StringArray& files, int x, int y)
{
    dropTarget = -1;

    int tracknum = strip->getTrackIndex();
    int loop = getDropTarget(x, y);
    Trace(2, "StripLoopStack: filesDropped into track %d loop %d\n", tracknum, loop);

    AudioClerk* clerk = strip->getSupervisor()->getAudioClerk();
    // track/loop numbers are 1 based, with zero meaning "active"
    // strip->followTrack and our loop index are zero based
    clerk->filesDropped(files, tracknum + 1, loop + 1);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
