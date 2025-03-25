#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIConfig.h"
#include "../../model/Symbol.h"
#include "../../model/Session.h"

// so we can call filesDropped
#include "../../AudioClerk.h"

#include "../JuceUtil.h"
#include "../MobiusView.h"

#include "Colors.h"
#include "TrackStrip.h"
#include "StripElement.h"
#include "StripElements.h"

//////////////////////////////////////////////////////////////////////
//
// TrackNumber
//
//////////////////////////////////////////////////////////////////////

/**
 * This formerly also functioned as the focus lock widget
 * Might be nice to have that by clicking on it rather than making
 * them take up space with the FocusLock button, but clicking on
 * the number is also a very common way to select tracks with the mouse
 * so not sure...
 *
 * number vs. name
 * Old code displayed either the number or the track name if one was
 * set in the Setup.  Since names are variable, the preferred size
 * needs to be reasonably wide.
 */
StripTrackNumber::StripTrackNumber(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionTrackNumber)
{
    action.symbol = strip->getProvider()->getSymbols()->intern("FocusLock");
    action.setScopeTrack(parent->getTrackIndex() + 1);
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

void StripTrackNumber::update(MobiusView* view)
{
    (void)view;
    // it's easier to let the containing strip handle it since it knows the track index to follow
    MobiusViewTrack* track = strip->getTrackView();
    if (track->refreshName ||
        track->focused != focusLock) {

        focusLock = track->focused;
        repaint();
    }
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
    MobiusViewTrack* track = strip->getTrackView();

    juce::Colour textColor = juce::Colour(MobiusGreen);
    if (focusLock)
      textColor = juce::Colour(MobiusRed);
    else if (track->type == Session::TypeMidi)
      textColor = juce::Colour(MobiusPink);

    g.setColour(textColor);

    if (track->name.length() == 0) {
        juce::Font font(JuceUtil::getFont(getHeight()));

        g.setFont(font);

        // if we're docked, the TrackStrip has the number
        // otherwise update must have remembered the active track

        g.drawText(juce::String(strip->getTrackIndex() + 1), 0, 0, getWidth(), getHeight(),
                   juce::Justification::centred);
    }
    else {
        juce::Font font(JuceUtil::getFont(getHeight()));
        // hacking around the unpredictable truncation, if the name is beyond
        // a certain length, reduce the font height
        if (track->name.length() >= 10)
          font = JuceUtil::getFontf(getHeight() * 0.75f);
          
        // not sure about font sizes, we're going to use fit so I think
        // that will size down as necessary

        g.setFont(font);
        g.drawFittedText(track->name, 0, 0, getWidth(), getHeight(),
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
        strip->getProvider()->doAction(&action);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Master
//
//////////////////////////////////////////////////////////////////////

StripMaster::StripMaster(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionMaster)
{
    action.symbol = strip->getProvider()->getSymbols()->intern("SyncMasterTrack");
    action.setScopeTrack(parent->getTrackIndex() + 1);
}

StripMaster::~StripMaster()
{
}

int StripMaster::getPreferredWidth()
{
    return 180;
}

int StripMaster::getPreferredHeight()
{
    return 30;
}

void StripMaster::update(MobiusView* view)
{
    (void)view;
    // it's easier to let the containing strip handle it since it knows the track index to follow
    MobiusViewTrack* track = strip->getTrackView();
    if (track->refreshName ||
        track->trackSyncMaster != trackSyncMaster ||
        track->transportMaster != transportMaster) {

        trackSyncMaster = track->trackSyncMaster;
        transportMaster = track->transportMaster;
        repaint();
    }
}

void StripMaster::paint(juce::Graphics& g)
{

    int textHeight = 12;
    juce::Font font(JuceUtil::getFont(textHeight));
    g.setFont(font);
        
    int method = 2;
   
    if (method == 1) {
        // first attempt
        // Master: Track Transport
        // with each of those two words visible and highlighted
        // in theory each word could be clickable to turn it on and off

        // something weird going on with left, need to push it to get past the border
        // and even after the normal border width it still truncates on the left
        // 12 is the absoute minimum
        int left = 18;
        int fieldWidth = 50;
        g.setColour(juce::Colour(MobiusGreen));
        g.drawText("Master:", left, 0, fieldWidth, getHeight(), juce::Justification::centredRight);

        left += (fieldWidth + 4);

        if (trackSyncMaster)
          g.setColour(juce::Colour(MobiusYellow));
        else
          g.setColour(juce::Colour(MobiusBlue));
    
        fieldWidth = 30;

        g.drawText("Track", left, 0, fieldWidth, getHeight(), juce::Justification::centredLeft);
    
        left += (fieldWidth + 4);
    
        if (transportMaster)
          g.setColour(juce::Colour(MobiusYellow));
        else
          g.setColour(juce::Colour(MobiusBlue));

        fieldWidth = 50;
        g.drawText("Transport", left, 0, fieldWidth, getHeight(), juce::Justification::centredLeft);
    }
    else {
        // method two
        // Track Master | Transport Master | Track+Trans Master
        // single word that shows when enabled and invisible when disabled
        // not clickable
        juce::String status;
        if (trackSyncMaster && transportMaster)
          status = "Track/Trans Master";
        else if (trackSyncMaster)
          status = "Track Master";
        else if (transportMaster)
          status = "Transport Master";

        // clearing the ful width trashes the focus border
        // need to be insetting the entire strip at a higher level!
        juce::Rectangle<int> area = getLocalBounds();
        // something really weird is going on here with the strip width
        // the inset needs to be abnormally large to prevent clipping the edges
        // of the focus box, this is doing sizing all wrong, each strip needs to have
        // a size with clipping within it, and things are extending outside those bounds
        // or better yet, let the strips be of their preferred width and put them
        // in a Viewport that can scroll horizontally
        int inset = 32;
        area = area.withTrimmedLeft(inset);
        area = area.withTrimmedRight(inset);
        g.setColour(juce::Colours::black);
        g.fillRect(area);
        //g.setColour(juce::Colours::white);
        //g.drawRect(area);

        if (status.length() > 0) {
            g.setColour(juce::Colour(MobiusYellow));
            g.drawText(status, area, juce::Justification::centred);
 }
    }
}

void StripMaster::mouseDown(const class juce::MouseEvent& event)
{
    if (!strip->isActive()) {
        // superclass will activate it
        StripElement::mouseDown(event);
    }
    else {
        //strip->getProvider()->doAction(&action);
    }
}

//////////////////////////////////////////////////////////////////////
//
// GroupName
//
//////////////////////////////////////////////////////////////////////

StripGroupName::StripGroupName(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionGroupName)
{
}

StripGroupName::~StripGroupName()
{
}

int StripGroupName::getPreferredWidth()
{
    // this is what we started with when just displaying a number
    // make it wider for names
    //return 60;
    return 180;
}

int StripGroupName::getPreferredHeight()
{
    return 30;
}

void StripGroupName::update(MobiusView* view)
{
    MobiusViewTrack* track = view->getTrack(strip->getTrackIndex());
    if (track->refreshGroup)
      repaint();
}


/**
 * See StripTrackNumber for comments about drawFittedText
 */
void StripGroupName::paint(juce::Graphics& g)
{
    MobiusViewTrack* track = strip->getTrackView();
    
    juce::Colour textColor = juce::Colour(MobiusGreen);
    if (track->groupColor != 0)
      textColor = juce::Colour(track->groupColor);
    g.setColour(textColor);
   
    if (track->groupName.length() > 0) {
        juce::Font font(JuceUtil::getFont(getHeight()));
        // hacking around the unpredictable truncation, if the name is beyond
        // a certain length, reduce the font height
        if (track->groupName.length() >= 10)
          font = JuceUtil::getFontf(getHeight() * 0.75f);
          
        // not sure about font sizes, we're going to use fit so I think
        // that will size down as necessary

        g.setFont(font);
        g.drawFittedText(track->groupName, 0, 0, getWidth(), getHeight(),
                         juce::Justification::centred,
                         1, // max lines
                         1.0f);
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
    action.symbol = strip->getProvider()->getSymbols()->intern("FocusLock");
    // TrackStrip track numbers are zero based, should call
    // this TrackIndex!
    action.setScopeTrack(parent->getTrackIndex() + 1);
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

void StripFocusLock::update(MobiusView* view)
{
    MobiusViewTrack* track = view->getTrack(strip->getTrackIndex());
    if (track->focused != focusLock) {
        focusLock = track->focused;
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
    
    strip->getProvider()->doAction(&action);
}

//////////////////////////////////////////////////////////////////////
//
// LoopRadar
//
//////////////////////////////////////////////////////////////////////

const int LoopRadarDefaultDiameter = 30;
const int LoopRadarPadding = 4;

StripLoopRadar::StripLoopRadar(class TrackStrip* parent) :
    StripElement(parent, StripDefinitionLoopRadar)
{
}

StripLoopRadar::~StripLoopRadar()
{
}

void StripLoopRadar::configure()
{
    UIConfig* config = strip->getProvider()->getUIConfig();
    diameter = config->getInt("radarDiameter");
    if (diameter == 0)
      diameter = LoopRadarDefaultDiameter;
}

int StripLoopRadar::getPreferredWidth()
{
    
    return diameter + (LoopRadarPadding * 2);
}

int StripLoopRadar::getPreferredHeight()
{
    return diameter + (LoopRadarPadding * 2);
}

void StripLoopRadar::update(MobiusView* view)
{
    MobiusViewTrack* track = view->getTrack(strip->getTrackIndex());
    
    juce::Colour color = Colors::getLoopColor(track);

    if (track->frame != loopFrame ||
        track->frames != loopFrames ||
        color != loopColor) {
        
        loopFrame = track->frame;
        loopFrames = track->frames;
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
                           (float)diameter, (float)diameter,
                           startrad, endrad, (float)innerCircle);

        g.setColour(loopColor);
        g.fillPath(path);
    }
    else {
        // color shold have been left red if recording
        g.setColour(loopColor);
        g.fillEllipse((float)LoopRadarPadding, (float)LoopRadarPadding,
                      (float)diameter, (float)diameter);
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

void StripLoopThermometer::update(MobiusView* view)
{
    MobiusViewTrack* track = view->getTrack(strip->getTrackIndex());

    if (track->frame != loopFrame ||
        track->frames != loopFrames) {

        loopFrame = track->frame;
        loopFrames = track->frames;
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

void StripOutputMeter::update(MobiusView* view)
{
    MobiusViewTrack* track = view->getTrack(strip->getTrackIndex());
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

void StripInputMeter::update(MobiusView* view)
{
    MobiusViewTrack* track = view->getTrack(strip->getTrackIndex());
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
    UIConfig* config = strip->getProvider()->getUIConfig();
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
 * so it is harder to do difference detection due to potential canges
 * inactive loops.   MobiusView now handles most of that and sets these
 * flags:
 *
 *     refreshSwitch
 *        the loop number of the next loop has changed, this requires
 *        redrawing a highlight around the target loop which is currently
 *        inactive
 *
 *     refreshLoopContent
 *       some form of loading happened into a loop that was inactive
 *       and possibly empty, since we draw empty vs. full loops differently
 *       need to refresh the stack
 *
 */
void StripLoopStack::update(MobiusView* view)
{
    MobiusViewTrack* track = view->getTrack(strip->getTrackIndex());
    
    if (track->refreshSwitch ||
        track->refreshLoopContent ||
        trackLoops != track->loopCount ||
        lastActive != track->activeLoop ||
        dropTarget != lastDropTarget) {

        trackLoops = track->loopCount;
        lastActive = track->activeLoop;
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
    MobiusViewTrack* track = strip->getTrackView();

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
    if (track->nextLoopNumber > 0)
      switchDestination = track->nextLoopNumber - 1;
    else if (track->returnLoopNumber > 0)
      switchDestination = track->returnLoopNumber - 1;
    
    for (int row = 0 ; row < maxLoops ; row++) {

        int loopIndex = origin + row;
        if (loopIndex >= trackLoops) {
            // nothing more to display
            break;
        }

        MobiusViewLoop* loop = track->getLoop(loopIndex);

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
            if (hoverTarget)
              g.setColour(juce::Colours::grey);
            else
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
                color = Colors::getLoopColor(track);
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
    dropTarget = getDropTarget(x, y);
    //Trace(2, "LoopStack::fileDragEnter");
}

void StripLoopStack::fileDragMove(const juce::StringArray& files, int x, int y)
{
    (void)files;
    dropTarget = getDropTarget(x, y);
    //Trace(2, "LoopStack::fileDragMove");
}

void StripLoopStack::fileDragExit(const juce::StringArray& files)
{
    (void)files;
    dropTarget = -1;
    //Trace(2, "LoopStack::fileDragExit");
}

void StripLoopStack::filesDropped(const juce::StringArray& files, int x, int y)
{
    dropTarget = -1;

    int tracknum = strip->getTrackIndex();
    int loop = getDropTarget(x, y);
    Trace(2, "StripLoopStack: filesDropped into track %d loop %d\n", tracknum, loop);

    AudioClerk* clerk = strip->getProvider()->getAudioClerk();
    // track/loop numbers are 1 based, with zero meaning "active"
    // strip->followTrack and our loop index are zero based
    // this handles both audio and MIDI files
    clerk->filesDropped(files, tracknum + 1, loop + 1);
}

/**
 * Would like to highlight loop rows as you hover over them to indiciate
 * you can click on them, independent of fileDragEnter.
 * Was wondering if there would be a double notification betwene mouseEnter
 * and fileDragEnter and there does not appear to be.  fileDragEnter has priority
 * and mouseEnter will not be called.  So we can set the dropTarget to get the repaint
 * to trigger, but also set hoverTarget so we can distinguish it from a file drop by color.
 */
void StripLoopStack::mouseEnter(const juce::MouseEvent& e)
{
    dropTarget = getDropTarget(e.x, e.y);
    hoverTarget = true;
    //Trace(2, "LoopStack::mouseEnter");
}

void StripLoopStack::mouseMove(const juce::MouseEvent& e)
{
    dropTarget = getDropTarget(e.x, e.y);
    //Trace(2, "LoopStack::mouseMove");
}

void StripLoopStack::mouseExit(const juce::MouseEvent& e)
{
    (void)e;
    dropTarget = -1;
    hoverTarget = false;
    //Trace(2, "LoopStack::mouseExit");
}

/**
 * Allow mouseDown to change loops.
 */
void StripLoopStack::mouseDown(const juce::MouseEvent& e)
{
    // need this, or are they in the MouseEvent?
    juce::ModifierKeys mods = juce::ModifierKeys::getCurrentModifiers();
    bool lmb = e.mods.isLeftButtonDown();
    bool rmb = e.mods.isRightButtonDown();
    const char* bstr = "???";
    if (lmb)
      bstr = "left";
    else if (rmb)
      bstr = "right";
    
    int target = getDropTarget(e.getMouseDownX(), e.getMouseDownY());
    MobiusViewTrack* track = strip->getTrackView();
    Provider* provider = strip->getProvider();
    int trackNumber = track->index + 1;
    int loopNumber = target + 1;
    
    // use e.mods.isLeftButtonDown or isRightButtonDown if you want to distinguish buttons
    if (mods.isAltDown()) {
        if (track->type == Session::TypeMidi)
          provider->dragMidi(trackNumber, loopNumber);
        else
          provider->dragAudio(trackNumber, loopNumber);
    }
    else if (mods.isCtrlDown()) {
        if (mods.isShiftDown()) {
            if (track->type == Session::TypeMidi)
              provider->saveMidi(trackNumber, loopNumber);
            else
              provider->saveAudio(trackNumber, loopNumber);
        }
        else {
            if (track->type == Session::TypeMidi)
              provider->loadMidi(trackNumber, loopNumber);
            else
              provider->loadAudio(trackNumber, loopNumber);
        }
    }
    else {
        // Trace(2, "StripLoopStack: Mouse %s down over loop %d", bstr, target);
        // this I want to be treated as a loop switch
        if (track->activeLoop != target) {
            UIAction a;
            a.symbol = provider->getSymbols()->getSymbol(FuncSelectLoop);
            a.value = loopNumber;
            a.setScopeTrack(trackNumber);
            strip->getProvider()->doAction(&a);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
