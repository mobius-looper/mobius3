
#include <JuceHeader.h>

#include "MobiusView.h"

MobiusView::MobiusView()
{
    // start this off with one track so we don't have
    // to make everything check the track pointer during initialization
    // before it has been refreshed
    MobiusViewTrack* vt = new MobiusViewTrack();
    tracks.add(vt);
    track = vt;
}

/**
 * Put the view into a known empty state.
 */
void MobiusView::reset()
{
    trackCount = 0;
    track = nullptr;
    for (auto t : tracks) {
        t->forceRefresh = false;
        t->number = 0;
        t->name = "";
        t->refreshName = false;
        t->midi = false;
        t->groupOrdinal = -1;
        t->groupName = "";
        t->groupColor = 0;
        t->focused = false;
        t->inputLevel = 0;
        t->outputLevel = 0;
        t->feedback = 0;
        t->altFeedback = 0;
        t->pan = 0;
        t->solo = false;
        t->inputMonitorLevel = 0;
        t->outputMonitorLevel = 0;
        t->loopCount = 0;
        t->activeLoop = 0;
        t->mode = "";
        t->refreshMode = false;
        t->minorModes.clear();
        t->refreshMinorModes = false;
        t->recording = false;
        t->mute = false;
        t->pause = false;
        t->reverse = false;
        t->frames = 0;
        t->frame = 0;
        t->subcycle = 0;
        t->subcycles = 0;
        t->cycle = 0;
        t->cycles = 0;
        t->nextLoop = 0;
        t->returnLoop = 0;
        t->beatLoop = false;
        t->beatCycle = false;
        t->beatSubcycle = false;
        t->windowOffset = 0;
        t->windowHistoryFrames = 0;

        // don't need to clear it as long as count goes zero
        //t->loops.clear();
        
        t->refreshLayers = false;
        t->layerCount = 0;
        t->activeLayer = 0;
        t->checkpoints.clear();
    
        t->refreshEvents = false;
        //t->events.clear();
    }
}
