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
#include "../../model/MobiusState.h"
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

/**
 * Notified when either the MobiusConfig or UIConfig changes.
 * If this is during initialization, allocate the track array,
 * after that you have to restart to change track count.
 */
void TrackStrips::configure()
{
    if (tracks.size() == 0) {
        // we're initializing
        MobiusConfig* config = display->getSupervisor()->getMobiusConfig();
        int trackCount = config->getTracks();
        if (trackCount == 0)
          trackCount = 8;
        
        for (int i = 0 ; i < trackCount ; i++) {
            TrackStrip* strip = new TrackStrip(this);
            strip->setFollowTrack(i);
            tracks.add(strip);
            addAndMakeVisible(strip);
        }

        // decided to simplify this to just a dualRows boolean since it
        // can only ever be 1 or 2
        UIConfig* uiconfig = display->getSupervisor()->getUIConfig();
        int rows = uiconfig->getInt("trackRows");
        dualTracks = (rows == 2);
    }

    for (auto strip : tracks)
      strip->configure();
}

void TrackStrips::update(MobiusState* state)
{
    for (int i = 0 ; i < tracks.size() ; i++) {
        TrackStrip* strip = tracks[i];
        strip->update(state);
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
