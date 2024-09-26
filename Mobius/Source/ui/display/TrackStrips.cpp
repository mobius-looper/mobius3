/**
 * Container of docked track strips.
 *
 * todo: Mobius core doesn't adapt to change in the track count until restart.
 * It's a little easier here, but we've got child components to deal with
 * adding and removing.  Until we can get both sides in sync, only
 * respond to track counts on startup.
 */

#include <JuceHeader.h>

#include "../../model/UIConfig.h"
#include "../../model/MobiusConfig.h"
#include "../../Supervisor.h"

#include "MobiusDisplay.h"
#include "TrackStrip.h"
#include "TrackStrips.h"

TrackStrips::TrackStrips(MobiusDisplay* parent)
{
    setName("TrackStrips");
    display = parent;
}

TrackStrips::~TrackStrips()
{
}

Supervisor* TrackStrips::getSupervisor()
{
    return display->getSupervisor();
}

class MobiusView* TrackStrips::getMobiusView()
{
    return display->getMobiusView();
}

/**
 * Notified when either the MobiusConfig or UIConfig changes.
 * With the introduction of MIDI tracks, the number of tracks can
 * grow or shrink as tracks are configured.  Eventually audio
 * tracks should allow this but they can't right now.  For display
 * purposes, it doesn't really matter what the tracks underneath are.
 */
void TrackStrips::configure()
{
    MobiusView* view = display->getSupervisor()->getMobiusView();
    int trackCount = view->totalTracks;

    // prevent crashes
    if (trackCount == 0) {
        Trace(1, "TrackStrips: Got here with empty tracks, what's the deal");
        trackCount = 1;
    }
    
    // technically should repaint if tracks were changed from audio to midi
    // without changing the number of them
    bool needsRefresh = false;
    if (trackCount > tracks.size()) {
        for (int i = tracks.size() ; i < trackCount ; i++) {
            TrackStrip* strip = new TrackStrip(this);
            strip->setFollowTrack(i);
            tracks.add(strip);
            addAndMakeVisible(strip);
        }
        needsRefresh = true;
    }
    else if (trackCount < tracks.size()) {
        while (tracks.size() != trackCount) {
            TrackStrip* strip = tracks[trackCount];
            removeChildComponent(strip);
            tracks.removeObject(strip, true);
        }
        needsRefresh = true;
    }
    
    // decided to simplify this to just a dualRows boolean since it
    // can only ever be 1 or 2
    UIConfig* uiconfig = display->getSupervisor()->getUIConfig();
    int rows = uiconfig->getInt("trackRows");
    bool needsDual = (rows == 2);
    if (needsDual != dualTracks)
      needsRefresh = true;
    dualTracks = needsDual;

    for (auto strip : tracks)
      strip->configure();

    if (needsRefresh) {
        // repaint() isn't enough, it needs to have a full resized()
        // to regenerate the layout
        resized();
    }
}

void TrackStrips::update(class MobiusView* view)
{
    for (int i = 0 ; i < tracks.size() ; i++) {
        TrackStrip* strip = tracks[i];
        strip->update(view);
    }
}

int TrackStrips::getPreferredHeight()
{
    int preferred = 0;
    // these are all the same so just look at the first one
    if (tracks.size() > 0)
      preferred = tracks[0]->getPreferredHeight();

    if (dualTracks) {
        // but we can now divide them
        preferred *= 2;
    }
    
    return preferred;
}

int TrackStrips::getPreferredWidth()
{
    int preferred = 0;
    // these are all the same so just look at the first one
    if (tracks.size() > 0) {
        int oneWidth = tracks[0]->getPreferredWidth();
        preferred = oneWidth * tracks.size();
    }

    if (dualTracks) {
        // does this actually matter?
        // the containing window will be whatever it is
        // and we'll resize accordingly
        // what we really need here is to have a minimum width
        // and have dualTracks take effect only if we overflow that
    }
    
    return preferred;
}

/**
 * These are normally just spread over a single row at the bottom
 * of the main window.  If dualRows is on, they are split into two rows
 * which almost no one will want unless they have a very large number of tracks.
 * Since this will take a large amount of space away from the status area,
 * I don't think it is very useful.  A viewport that scrolls would be better?
 */
void TrackStrips::resized()
{
    if (tracks.size() > 0) {

        if (!dualTracks) {
            // original layout that I know works
            int height = getHeight();
            int oneWidth = tracks[0]->getPreferredWidth();
            int leftOffset = 0;

            // a few options: spread out to fill or compress and center
            bool spread = true;
            if (spread) {
                oneWidth = getWidth() / tracks.size();
            }
            else {
                int indent = (getWidth() - (oneWidth * tracks.size()) / 2);
                leftOffset = (indent >= 0) ? indent : 0;
            }
        
            for (auto strip : tracks) {
                strip->setBounds(leftOffset, 0, oneWidth, height);
                leftOffset += oneWidth;
            }
        }
        else {
            // experimental multi-row layout
            int rowHeight = getHeight() / 2;
            // may round off and the second will include the odd size
            int tracksPerRow = tracks.size() / 2;
            int squishWidth = getWidth() / tracksPerRow;
            int preferred = tracks[0]->getPreferredWidth();
            int oneWidth = squishWidth;
            if (preferred < squishWidth)
              oneWidth = preferred;
            int leftOffset = 0;
            int topOffset = 0;

            for (int i = 0 ; i < tracks.size() ; i++) {
                if (i == tracksPerRow) {
                    // move to the next one
                    topOffset += rowHeight;
                    leftOffset = 0;
                    int remaining = tracks.size() - tracksPerRow;
                    oneWidth = getWidth() / remaining;
                }

                TrackStrip* strip = tracks[i];
                strip->setBounds(leftOffset, topOffset, oneWidth, rowHeight);
                leftOffset += oneWidth;
            }
        }
    }
}

void TrackStrips::paint(juce::Graphics& g)
{
    (void)g;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
