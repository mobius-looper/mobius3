
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MobiusView.h"

MobiusView::MobiusView()
{
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
