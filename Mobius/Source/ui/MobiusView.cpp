
#include <JuceHeader.h>

#include "MobiusView.h"

/**
 * Put the view into a known empty state.
 */
void MobiusView::reset()
{
    trackCount = 0;
    track = nullptr;
    for (auto t : tracks) {
        t->number = 0;
        t->name = "";
        t->refreshName = false;
        t->midi = false;
        t->groups.clear();
        t->focused = false;
        t->inputLevel = 0;
        t->outputLevel = 0;
        t->feedback = 0;
        t->altFeedback = 0;
        t->pan = 0;
        t->solo = false;
        t->inputMonitorLevel = 0;
        t->outputMonitorLevel = 0;
        t->loopNumber = 0;
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
        t->windowOffset;
        t->historyFrames;
        
        t->layerCount = 0;
        t->activeLayer = 0;
        t->refreshLayers = false;
    
        t->loopCount = 0;
        // don't need to clear it as long as count goes zero
        //t->loops.clear();
        t->checkpoints.clear();
        
        t->eventCount = 0;
        //t->events.clear();
        t->refreshEvents = false;
    }
}
