
#include <JuceHeader.h>

#include "../util/Trace.h"

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
 * Return the view for a track with the given index.
 * If index is -1 return the active track.
 * If out of range, which should not happen, return the active
 * track so we don't crash.
 */
MobiusViewTrack* MobiusView::getTrack(int index)
{
    MobiusViewTrack* result = nullptr;
    
    if (index >= 0 && index < tracks.size()) {
        result = tracks[index];
    }
    else {
        // -1 means return the active track
        // if it's out of range, also return the active track
        // if we don't have one, then we're in a weird initialization state
        // do NOT return nullptr
        if (track == nullptr) {
            Trace(1, "MobiusView: getTrack uninitialized track list");
            track = new MobiusViewTrack();
            tracks.add(track);
        }
        if (index >= 0) {
            // this didn't use the negative convention for active track
            // so the index is out of range
            Trace(1, "MobiusView: getTrack invalid index");
        }
        result = track;
    }
    return result;
}

MobiusViewLoop* MobiusViewTrack::getLoop(int index)
{
    MobiusViewLoop* result = nullptr;
    if (index >= 0 && index < loops.size()) {
        result = loops[index];
    }
    else {
        // shouldn't happen, misconfiguration, don't crash
        Trace(1, "MobiusView: getLoop invalid index");
        if (loops.size() == 0) {
            Trace(1, "MobiusView: getLoop uninitialized loop list");
            result = new MobiusViewLoop();
            loops.add(result);
        }
        result = loops[0];
    }
    return result;
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
        t->nextLoopNumber = 0;
        t->returnLoopNumber = 0;
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
