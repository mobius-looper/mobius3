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

/**
 * Notified when either the MobiusConfig or UIConfig changes.
 * If this is during initialization, allocate the track array,
 * after that you have to restart to change track count.
 */
void TrackStrips::configure()
{
    if (tracks.size() == 0) {
        // we're initializing
        MobiusConfig* config = Supervisor::Instance->getMobiusConfig();
        int trackCount = config->getTracks();
        if (trackCount == 0 || trackCount > 16) {
            trackCount = 8;
        }
        
        for (int i = 0 ; i < trackCount ; i++) {
            TrackStrip* strip = new TrackStrip(this);
            strip->setFollowTrack(i);
            tracks.add(strip);
            addAndMakeVisible(strip);
        }
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
    return preferred;
}

void TrackStrips::resized()
{
    if (tracks.size() > 0) {
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
}

void TrackStrips::paint(juce::Graphics& g)
{
    (void)g;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
