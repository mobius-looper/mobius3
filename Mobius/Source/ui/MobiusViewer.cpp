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
    
    // initialize the Query we use to dig out the runtime subcycles
    // parameter value
    subcyclesQuery.symbol = s->getSymbols()->intern("subcycles");
}

MobiusViewer::~MobiusViewer()
{
}

/**
 * The root of the periodic full refresh.
 *
 * This is expected to be called once every 1/10th second by the
 * maintenance thread. It does not update the UI, it only refreshes
 * the model and sets various flags when something that is more complex
 * changes so the UI can optimize out repaints when nothing is changing.
 */
void MobiusViewer::refresh(MobiusInterface* mobius, MobiusView* view)
{
    // the old state object, which has it's own refresh interval
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
        refreshTrack(state, tstate, view, tview, active);
        
        if (active)
          view->track = tview;
    }

}


/**
 * Refresh a track
 *
 * If the active flag is not on, then we don't need to
 * do a full refrsh of events, layers, and some other things.
 * Anything that might be used by track strip elements needs to
 * always be refreshed.
 */
void MobiusViewer::refreshTrack(MobiusState* state, MobiusTrackState* tstate,
                                MobiusView* mview, MobiusViewTrack* tview,
                                bool active)
{
    refreshTrackName(state, tstate, mview, tview);
    refreshInactiveLoops(tstate, tview);
    refreshTrackProperties(tstate, tview);
    
    MobiusLoopState* lstate = &(tstate->loops[tstate->activeLoop]);
    refreshActiveLoop(tstate, lstate, active, tview);
    
    refreshMode(tstate, tview);
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
                                    MobiusView* mview, MobiusViewTrack* tview)
{
    if (mview->setupOrdinal != state->setupOrdinal) {

        tview->name = "";
        MobiusConfig* config = supervisor->getMobiusConfig();
        Setup* setup = config->getSetup(state->setupOrdinal);
        if (setup != nullptr) {
            // "numbers" are 1 based "indexes" are 0 based
            SetupTrack* st = setup->getTrack(tstate->number - 1);
            if (st != nullptr)
              tview->name = juce::String(st->getName());
        }
        mview->setupOrdinal = state->setupOrdinal;
        tview->refreshName = true;
    }
}

/**
 * Refresh various properties of each track, not related to
 * a particular loop.
 */
void MobiusViewer::refreshTrackProperties(MobiusTrackState* tstate, MobiusViewTrack* tview)
{
    tview->focused = tstate->focusLock;
    tview->inputLevel = tstate->inputLevel;
    tview->outputLevel = tstate->outputLevel;
    tview->feedback = tstate->feedback;
    tview->altFeedback = tstate->altFeedback;
    tview->pan = tstate->pan;
    tview->solo = tstate->solo;
    tview->inputMonitorLevel = tstate->inputMonitorLevel;
    tview->outputMonitorLevel = tstate->outputMonitorLevel;

    refreshTrackGroups(tstate, tview);
}

/**
 * Refresh the group(s) a track can belong to.
 * Currently a group can only be a member of one group, but that will
 * change in the future.
 *
 * The group a core track is in is identified by the "group ordinal" which
 * is 1 based in MobiusState with 0 meaning that the track is not assigned
 * to a group.
 *
 * The names now come from the GroupDefinition objects.
 * Ideally, I'd like to get group assignments out of core and make it
 * purely a UI thing with Binderator handling the replication.
 *
 * !! also need to detect when GroupDefinitions change which can change
 * the name but not the assigned ordinals.  Two possibilities:
 *
 *    - keep a runtime version number that gets incremented on any edit
 *      similar to what we should be doing for Setups
 *    - have the configuration UI call back to the viewer to reset the last
 *      known state so we trigger a diff next time
 */
void MobiusViewer::refreshTrackGroups(MobiusTrackState* tstate,  MobiusViewTrack* tview)
{
    int newNumber = tstate->group;
    
    if (tview->groupOrdinal != newNumber) {
        tview->groupOrdinal = newNumber;

        // could just make the display work from the ordinal, but we might
        // as well go get the name/color to make it easier
        tview->groupName = "";
        tview->groupColor = 0;
        
        MobiusConfig* config = supervisor->getMobiusConfig();
        
        // ignore if out of range
        if (newNumber > 0 && newNumber <= config->groups.size()) {
            GroupDefinition* group = config->groups[newNumber - 1];
            tview->groupName = group->name;
            tview->groupColor = group->color;
        }

        tview->refreshGroup = true;
    }
}

/**
 * Inactive Loops
 *
 * This is primarily for the LoopStack strip element.
 * The main Mobius display only shows state for the active loop in the
 * active track.  The LoopStack is shown for every track and has a small
 * amount of information about the inactive loops in each track for
 * highlighting.
 *
 * The old model called these the "loop summaries" which was merged
 * with MobiusLoopState during 3 development.
 *
 * The loop view array starts out at zero and grows over time as the
 * loop count in the track increases.  
 */
void MobiusViewer::refreshInactiveLoops(MobiusTrackState* tstate, MobiusViewTrack* tview)
{
    tview->loopCount = tstate->loopCount;
    tview->activeLoop = tstate->activeLoop;
    
    for (int i = 0 ; i < tstate->loopCount ; i++) {
        MobiusLoopState* lstate = &(tstate->loops[i]);
        MobiusViewLoop* lview;
        if (i < tview->loops.size())
          lview = tview->loops[i];
        else {
            lview = new MobiusViewLoop();
            tview->loops.add(lview);
        }

        // there isn't much to say other than whether it is empty or not
        lview->frames = lstate->frames;

        // old model has these flags, but I don't think we need them
        // since the active track state will have the loop number
        // and the number of the loops that are the target of a switch

        //lview->active = (i == tstate->activeLoop);
        //lview->pending = (i == (tview->nextLoop - 1));
    }
}

/**
 * Refresh state related to the active loop in a track.
 * This is the majority of the view and is displayed by
 * the main display area, as well as the track strips.
 *
 * If this is not the active track, can suppress some details
 * like the scheduled loop events and loop layers since those will
 * not be shown in the track strips.
 */
void MobiusViewer::refreshActiveLoop(MobiusTrackState* tstate, MobiusLoopState* lstate,
                                     bool activeTrack, MobiusViewTrack* tview)
{
    tview->activeLoop = tstate->activeLoop;

    // things important for both the main display and the track strips
    tview->recording = lstate->recording;
    tview->pause = lstate->paused;
    tview->frames = lstate->frames;
    tview->frame = lstate->frame;

    // things below this are relevant only if this is the active track
    // since they are displayed in the main display and not the track strips

    // loop location and transitions
    tview->subcycle = lstate->subcycle;
    tview->cycle = lstate->cycle;
    tview->cycles = lstate->cycles;

    // this requires a query
    if (activeTrack)
      refreshSubcycles(tview);

    // I want all numbered things to be represented as 0 based indexes
    // in the view since that's what it mostly deals with, only if it
    // contains the word "number" is it 1 based
    // the old state model uses 1 based numbers here
    tview->nextLoop = lstate->nextLoop - 1;
    tview->returnLoop = lstate->returnLoop - 1;

    if (activeTrack)
      refreshMinorModes(tstate, lstate, tview);

    // latching beaters
    if (activeTrack) {
        tview->beatSubcycle = lstate->beatSubCycle;
        tview->beatCycle = lstate->beatCycle;
        tview->beatLoop = lstate->beatLoop;
    }
    
    // various
    tview->windowOffset = lstate->windowOffset;
    tview->historyFrames = lstate->historyFrames;

    if (activeTrack)
      refreshLayers(lstate, tview);

    if (activeTrack)
      refreshEvents(lstate, tview);
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

/**
 * Subcycles
 *
 * Refreshing the subcycles in a loop requires a query.
 * While this starts out with what is defined in the Preset it
 * can be changed dynamically at runtime.
 *
 * This is only necessary for the active loop to support the LoopMeter
 *
 */
void MobiusViewer::refreshSubcycles(MobiusViewTrack* tview)
{
    int subcycles = 0;
    if (supervisor->doQuery(&subcyclesQuery))
      subcycles = subcyclesQuery.value;

    if (subcycles == 0) {
        // this comes from the Preset, so something bad happened
        Trace(1, "MobiusViewer: Subcycles query came back zero\n");
        subcycles = 4;
    }

    tview->subcycles = subcycles;
}

/**
 * Layers
 * 
 * Unlike the old model, only care about layers in the active loop
 * the MobiusState layer model is insanely complicated thinking that
 * there needed to be a model for each layer, actually don't need anything
 * but checkpoint flags.  Sizes might be nice, bit a pita to maintain.
 */
void MobiusViewer::refreshLayers(MobiusLoopState* lstate, MobiusViewTrack* tview)
{
    tview->layerCount = lstate->layerCount + lstate->lostLayers + lstate->redoCount + lstate->lostRedo;
    tview->activeLayer = lstate->layerCount + lstate->lostLayers;

    // todo: checkpoint detection will be hard
    // a Bigint could be handy here
    // tview->checkpointsp.add(???
}

/**
 * Events
 */
void MobiusViewer::refreshEvents(MobiusLoopState* lstate, MobiusViewTrack* tview)
{
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
 * Minor Modes
 *
 * There are a LOT of minor modes, but the display element displays all of them
 * at once in a list.  So do refresh detection on the entire collection.
 *
 * Go ahead and assemble the value too, though that should be up to the element?
 */
void MobiusViewer::refreshMinorModes(MobiusTrackState* tstate, MobiusLoopState* lstate,
                                     MobiusViewTrack* tview)
{
    bool refresh = false;

    // "true when the global recorder is on"
    // wtf is this?
    // bool globalRecording;

    // note: both track state and loop state have a reverse flag
    // only pay attention to track state, the loop state has a "summary"
    // which might have been the reverse at the time it became inactive?

    if (tstate->reverse != tview->reverse) {
        tview->reverse =  tstate->reverse;
        refresh = true;
    }

    if (lstate->overdub != tview->overdub) {
        tview->overdub = lstate->overdub;
        refresh = true;
    }

    if (lstate->mute != tview->mute) {
        tview->mute = lstate->mute;
        refresh = true;
    }
    
    if (tstate->speedToggle != tview->speedToggle) {
        tview->speedToggle = tstate->speedToggle;
        refresh = true;
    }
    if (tstate->speedOctave != tview->speedOctave) {
        tview->speedOctave = tstate->speedOctave;
        refresh = true;
    }
    if (tstate->speedStep != tview->speedStep) {
        tview->speedStep = tstate->speedStep;
        refresh = true;
    }
    if (tstate->speedBend != tview->speedBend) {
        tview->speedBend = tstate->speedBend;
        refresh = true;
    }
    if (tstate->pitchOctave != tview->pitchOctave) {
        tview->pitchOctave = tstate->pitchOctave;
        refresh = true;
    }
    if (tstate->pitchStep != tview->pitchStep) {
        tview->pitchStep = tstate->pitchStep;
        refresh = true;
    }
    if (tstate->pitchBend != tview->pitchBend) {
        tview->pitchBend = tstate->pitchBend;
        refresh = true;
    }
    if (tstate->timeStretch != tview->timeStretch) {
        tview->timeStretch = tstate->timeStretch;
        refresh = true;
    }

    if (tstate->outSyncMaster != tview->outSyncMaster) {
        tview->outSyncMaster = tstate->outSyncMaster;
        refresh = true;
    }
	if (tstate->trackSyncMaster != tview->trackSyncMaster) {
        tview->trackSyncMaster = tstate->trackSyncMaster;
        refresh = true;
    }
    if (tstate->solo != tview->solo) {
        tview->solo = tstate->solo;
        refresh = true;
    }

    // why the fuck are these here?
    if (tstate->globalMute != tview->globalMute) {
        tview->globalMute = tstate->globalMute;
        refresh = true;
    }
    if (tstate->globalPause != tview->globalPause) {
        tview->globalPause = tstate->globalPause;
        refresh = true;
    }

    // loop windowing
    if (lstate->windowOffset != tview->windowOffset) {
        tview->windowOffset = lstate->windowOffset;
        refresh = true;
    }
    if (lstate->historyFrames != tview->historyFrames) {
        tview->historyFrames = lstate->historyFrames;
        refresh = true;
    }
        
    tview->refreshMinorModes = refresh;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
