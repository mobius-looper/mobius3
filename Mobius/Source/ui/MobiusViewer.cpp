/**
 * Translation between the old MobiusState and the new MobiusView,
 * plus additions and simpliciations.
 */

#include <JuceHeader.h>

#include "../Supervisor.h"

#include "../model/MobiusState.h"
#include "../model/UIEventType.h"
#include "../model/ModeDefinition.h"
#include "../model/MobiusConfig.h"

#include "../mobius/MobiusInterface.h"

#include "MobiusView.h"
#include "MobiusViewer.h"

MobiusViewer::MobiusViewer(Supervisor* s)
{
    supervisor = s;
}

MobiusViewer::~MobiusViewer()
{
}

/**
 * The root of the periodic full refresh
 */
void MobiusViewer::refresh(MobiusInterface* mobius, MobiusView* view)
{
    MobiusState* state = mobius->getState();
    
    // grow and refresh the tracks
    view->trackCount = state->trackCount;
    for (int i = 0 ; i < state->trackCount ; i++) {
        MobiusTrackState* tstate = &(state->tracks[i]);
        MobiusViewTrack* tview;
        if (i < view->tracks.size())
          tview = view->tracks[i];
        else {
            tview = new MobiusViewTrack();
            // number is 1 based
            tview->number = i + 1;
            tview->midi = false;
            view->tracks.add(tview);
        }
        
        bool active = (i == state->activeTrack);
        refreshTrack(state, tstate, active, tview);
        
        if (active)
          view->track = tview;
    }

}

/**
 * Refresh a track
 *
 * If the active flag is not on, then we don't need to
 * do a full refrsh of events, layers, and some other things.
 */
void MobiusViewer::refreshTrack(MobiusState* state, MobiusTrackState* tstate, bool active,
                                MobiusViewTrack* tview)
{
    (void)active;
    
    refreshTrackName(state, tstate, tview);
    refreshMode(tstate, tview);
    
    tview->loopCount = tstate->loopCount;

    // ensure capacity
    for (int i = tview->loops.size() ; i < tview->loopCount ; i++) {
        MobiusViewLoop* l = new MobiusViewLoop();
        tview->loops.add(l);
    }

    // state has the "group ordinal" which is 1 based with 0 meaning no group
    // continue until groups can be moved out of core
    // todo: get the GroupDefinition list, find the numbered definition
    // and copy the name
    // tview->groups = ???

    tview->focused = tstate->focusLock;
    tview->inputLevel = tstate->inputLevel;
    tview->outputLevel = tstate->outputLevel;
    tview->feedback = tstate->feedback;
    tview->altFeedback = tstate->altFeedback;
    tview->pan = tstate->pan;
    tview->solo = tstate->solo;
    tview->inputMonitorLevel = tstate->inputMonitorLevel;
    tview->outputMonitorLevel = tstate->outputMonitorLevel;

    // most of loop state is for the active loop and stored directly
    // on the track view
    
    MobiusLoopState* lstate = &(tstate->loops[tstate->activeLoop]);    
    // 1 based number of the active loop
    tview->loopNumber = lstate->number;

    // todo: minor modes are complex, do something nice

    tview->recording = lstate->recording;
    tview->mute = lstate->mute;
    tview->pause = lstate->paused;
    tview->reverse = lstate->reverse;
    
    tview->frames = lstate->frames;
    tview->frame = lstate->frame;
    tview->subcycle = lstate->subcycle;
    // todo: pull this from the parameter
    //tview->subcycles =  ???
    tview->cycle = lstate->cycle;
    tview->cycles = lstate->cycles;
    tview->nextLoop = lstate->nextLoop;
    tview->returnLoop = lstate->returnLoop;

    // todo: I don't think we need the "pending" flag in the old loop summary, this
    // would have to be flagged on the same loop represented by tview->nextLoop

    // latching beaters
    tview->beatSubcycle = lstate->beatSubCycle;
    tview->beatCycle = lstate->beatCycle;
    tview->beatLoop = lstate->beatLoop;

    // various
    tview->windowOffset = lstate->windowOffset;
    tview->historyFrames = lstate->historyFrames;
    
    // inactive loops
    // old model called these the "summary", newer model merged that
    // back in with MobiusLoopState, but it's still very sparse
    // organize this for the repaint of the loop stack
    for (int i = 0 ; i < tstate->loopCount ; i++) {
        MobiusViewLoop* vl = tview->loops[i];
        MobiusLoopState* inactive = &(tstate->loops[i]);
        vl->frames = inactive->frames;
        // could flag this so LoopStack doesn't have to work too hard?
        //vl->active = (lstate->number == tview->loopNumber);
        // ?? how does returnLoop relate to this?
        //vl->pending = (lstate->number == tview->nextLoop);
    }

    // layers
    // unlike the old model, only care about layers in the active loop
    // the MobiusState layer model is insanely complicated thinking that
    // there needed to be a model for each layer, actually don't need anything
    // but checkpoint flags, sizes might be nice, bit a pita to maintain
    tview->layerCount = lstate->layerCount + lstate->lostLayers + lstate->redoCount + lstate->lostRedo;
    tview->activeLayer = lstate->layerCount + lstate->lostLayers;

    // todo: checkpoint detection will be hard
    // a Bigint could be handy here
    // tview->checkpointsp.add(???
    
    // events
    for (int i = 0 ; i < lstate->eventCount ; i++) {
        MobiusEventState* estate = &(lstate->events[i]);
        MobiusViewEvent* ve;
        if (i < tview->events.size())
          ve = tview->events[i];
        else {
            ve = new MobiusViewEvent();
            tview->events.add(ve);
        }
        
        const char* newName = nullptr;
        if (estate->type != nullptr)
          newName = estate->type->getName();
        else
          newName = "???";

        // LoopMeter will display both the event type name and the argument
        // number for things like "LoopSwitch 2"
        if (strncmp(ve->name.toUTF8(), newName, strlen(newName)) ||
            ve->argument != estate->argument ||
            ve->frame != estate->frame || ve->pending != estate->pending) {

            ve->frame = estate->frame;
            ve->pending = estate->pending;
            ve->argument = estate->argument;
            ve->name = juce::String(newName) + " " + juce::String(ve->argument);
            tview->refreshEvents = true;
        }
    }

}

/**
 * Track name
 * 
 * These are not in MobiusTrackState, they are pulled from the active Setup.
 * Monitoring is awkward because we have to follow the active Setup ordinal
 * and the Setup version since you can edit the setup to change the name without
 * selecting a different one.
 *
 * !! how did pre-view refresh handle this?
 */
void MobiusViewer::refreshTrackName(MobiusState* state, MobiusTrackState* tstate, 
                                    MobiusViewTrack* tview)
{
    if (setupOrdinal != state->setupOrdinal) {

        tview->name = "";
        MobiusConfig* config = supervisor->getMobiusConfig();
        Setup* setup = config->getSetup(state->setupOrdinal);
        if (setup != nullptr) {
            // "numbers" are 1 based "indexes" are 0 based
            SetupTrack* st = setup->getTrack(tstate->number - 1);
            if (st != nullptr)
              tview->name = juce::String(st->getName());
        }
        setupOrdinal = state->setupOrdinal;
        tview->refreshName = true;
    }
}

/**
 * Major Mode
 *
 * The engine does not actually set UIPauseMode, the loop will be
 * in UIMuteMode with the paused flag set in the state
 * would be better if the engine just used the pseudo mode instead.
 *
 * Unclear what the priority of these is or if they can be combined
 * can you be in GlobalMute and Pause at the same time?
 * Favor Pause mode.
 *
 * Weirdly globalMute and globalPause are on the TrackState rather
 * than in the root state.
 */
void MobiusViewer::refreshMode(MobiusTrackState* tstate, MobiusViewTrack* tview)
{
    MobiusLoopState* loop = &(tstate->loops[tstate->activeLoop]);
    ModeDefinition* mode = loop->mode;

    if (tstate->globalMute) mode = UIGlobalMuteMode;
    if (loop->paused) mode = UIPauseMode;

    // seem to have lost conveyance of GlobalPause
    if (tstate->globalPause) mode = UIGlobalPauseMode;
    
    // shouldn't happen
    if (mode == nullptr) mode = UIResetMode;

    const char* newMode = mode->getName();
    if (strcmp(tview->mode.toUTF8(), newMode)) {
        tview->mode = juce::String(newMode);
        tview->refreshMode = true;
    }

    
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
