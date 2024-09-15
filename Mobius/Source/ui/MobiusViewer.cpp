/**
 * Translation between the old MobiusState and the new MobiusView,
 * plus additions and simpliciations.
 *
 * Most of the information is contained in the MobiusViewTrack which drives the display of both
 * the main Status Area in the center, and the list of Track Strips at the bottom.  The Status Area
 * is able to show everything about the track, the Track Strips will have a subset.  This distinction
 * is a minor optimization that should not be assumed to last forever.
 *
 * The refrsh process is assembling the view from several sources.  Most of it comes from MobiusState
 * which was the original way to communicate state from the engine to the UI.  Some is pulled from configuration
 * objects like Setup and GroupDefinition based on references in MoibusState.  And some is made by
 * querying the engine for a few real-time parameter values.
 *
 * Beyond the capture of engine state, the view also contains support for analyzing complex changes that
 * trigger a refresh of portions of the UI.  For many UI components it is enough to simply remember the last
 * displayed value and compare it with the current value and trigger a repaint if they differ.
 *
 * Some like MinorModesElement require an analysis of many things to produce the displayed result. Where different
 * detection is more than just comparison of old/new values, the logic is being moved here, making it easier to
 * modify the UI without losing the difference detection code.  The UI can test a few "refreshFoo" flags to
 * see if something needs to be refreshed.  These should be considered extensions of the view model that
 * are only there for convenience, not a fundamental part of the model.
 *
 * The process of periodic UI refresh must proceed like this:
 *
 *      - maintenance thread reaches a refresh interval
 *      - current MobiusState is obtained from the engine
 *      - refresh trigger flags in the view are cleared
 *      - the view is refreshed, trigger flags are set
 *      - the UI refresh scan is performed, this uses any combination
 *        of old/new values in the view and the refresh flags to determine
 *        whether repaint is necessary
 *      - the current view values are moved to the previous state for difference
 *        detection on the next cycle
 *
 * Components only get one pass to decide whether to repaint before refresh flags
 * are cleared and the difference state is moved for the next cycle.  In a few (one?)
 * cases, the refresh flags are "latching" and must be cleared by the UI components
 * themselves after they have repainted.  The only example right now is Beaters since
 * for reasons I'm forgetting, revisit this...
 *
 */

#include <JuceHeader.h>

#include "../Supervisor.h"

#include "../model/MobiusState.h"
#include "../model/UIEventType.h"
#include "../model/ModeDefinition.h"
#include "../model/MobiusConfig.h"
#include "../model/MainConfig.h"

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
 * Initialize the view at startup.
 *
 * Track counts are touchy.
 *
 * The number of audio tracks is controlled by MobiusConfig.tracks which is used
 * by the core during initialization.  The core does not respond to changes in track counts
 * until a restart, so once initialized the view must not change audio tracks.
 *
 * Midi tracks are allowed to be dynamic at runtime.
 *
 * The UI for the most part doesn't care if something is an audio or MIDI track, but the
 * UIActions sent to the core do.   At the moment, UIAction.scope has a single number space
 * and it must be possible to know if a given track number is an audio track or a midi track
 * for routing in the Kernel.
 *
 * The convention is that all audio tracks are first 1-n, and then all midi tracks
 * n+1-m.   Actions can then just use numbers 1-m and let the kernel sort out where they go.
 *
 * Since we reuse track view objects in the array once created, in fringe cases where you are
 * adding and removing tracks of different types this should be doing a better job of initializing
 * track views that become unused, then reused.  Can't happen yet.
 */
void MobiusViewer::initialize(MobiusView* view)
{
    MobiusConfig* config = supervisor->getMobiusConfig();
    view->audioTracks = config->getCoreTracks();

    // all things MIDI come from here
    MainConfig* main = supervisor->getMainConfig();
    view->midiTracks = main->getGlobals()->getInt("midiTracks");

    // stub this out till we're ready
    //view->midiTracks = 0;

    view->totalTracks = view->audioTracks + view->midiTracks;

    // flesh these out ahead of time to ensure there
    // is a correspondence between the logical track numbers and an object
    // in the view that matches that type
    for (int i = 0 ; i < view->audioTracks ; i++) {
        MobiusViewTrack* vt = new MobiusViewTrack();
        view->tracks.add(vt);
    }

    for (int i = 0 ; i < view->midiTracks ; i++) {
        MobiusViewTrack* vt = new MobiusViewTrack();
        vt->midi = true;
        view->tracks.add(vt);
    }

    // always start on the first one
    // this may conflict with the Setup on the first refresh
    view->focusedTrack = 0;
}

/**
 * The root of the periodic full refresh.
 *
 * This is expected to be called once every 1/10th second by the
 * maintenance thread. It does not update the UI, it only refreshes
 * the model and sets various flags when something that is more complex
 * changes so the UI can optimize out repaints when nothing is changing.
 */
void MobiusViewer::refresh(MobiusInterface* mobius, MobiusState* state, MobiusView* view)
{
    (void)mobius;

    // if state ever does get around to passing this back, whine if they're out of sync
    if (state->trackCount > 0 && state->trackCount != view->audioTracks)
      Trace(1, "MobiusViewer: Mismatched track counts in view and Mobius");

    // Counter needs this
    view->sampleRate = supervisor->getSampleRate();

    // move the track view to the one that has focus
    if (view->focusedTrack >= 0 && view->focusedTrack < view->tracks.size()) 
      view->track = view->tracks[view->focusedTrack];
    else
      Trace(1, "MobiusViewer: focusedTrack is out of whack");

    resetRefreshTriggers(view);

    // detect active Setup changes
    // do this before track refresh so our own refresh process can be optimized
    // this impacts refresh of the track names, maybe others
    // !! this is not enough, you can edit the setup and change the names but the
    // ordinal will stay the same, need to increment a version number on each edit
    // how to did pre-view code detect this?
    if (view->setupOrdinal != state->setupOrdinal) {
        view->setupChanged = true;
        view->setupOrdinal = state->setupOrdinal;
    }

    refreshAudioTracks(mobius, state, view);

    // MIDI Tracks are glued onto the end of the audio tracks
    refreshMidiTracks(mobius, view);
}

/**
 * Called at the beginning of every refresh cycle to reset the refresh
 * trigger flags.  Some components may clear these as a side effect of paint
 * but this should not be required.
 */
void MobiusViewer::resetRefreshTriggers(MobiusView* view)
{
    view->trackChanged = false;
    view->setupChanged = false;
    
    for (auto t : view->tracks) {
        t->refreshName = false;
        t->refreshGroup = false;
        t->loopChanged = false;
        t->refreshMode = false;
        t->refreshMinorModes = false;
        t->refreshLayers = false;
        t->refreshEvents = false;
        t->refreshSwitch = false;
        t->refreshLoopContent = false;
    }
}

/**
 * When ready for the initial display, force everything on since there
 * was no valid prior state to compare against.  Not all of these may
 * be necessary, but it gets the ball rolling.
 */
void MobiusViewer::forceRefresh(MobiusView* view)
{
    view->trackChanged = true;
    view->setupChanged = true;
    
    for (auto t : view->tracks) {
        t->refreshName = true;
        t->refreshGroup = true;
        t->loopChanged = true;
        t->refreshMode = true;
        t->refreshMinorModes = true;
        t->refreshLayers = true;
        t->refreshEvents = true;
        t->refreshSwitch = true;
        t->refreshLoopContent = true;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Audio Track Refresh
//
//////////////////////////////////////////////////////////////////////

void MobiusViewer::refreshAudioTracks(MobiusInterface* mobius, MobiusState* state, MobiusView* view)
{
    (void)mobius;

    for (int i = 0 ; i < view->audioTracks ; i++) {
        MobiusTrackState* tstate = &(state->tracks[i]);

        // obtain track view, growing if necessary
        MobiusViewTrack* tview;
        if (i < view->tracks.size())
          tview = view->tracks[i];
        else {
            tview = new MobiusViewTrack();
            // number for display is 1 based
            tview->number = i + 1;
            view->tracks.add(tview);
        }

        // only audio tracks have the concept of an active track
        // this is NOT the same as the view's focused track
        bool active = (i == state->activeTrack);
        tview->active = (i == state->activeTrack);

        // if this is the active track, extra refresh options are enabled
        // !! we actually don't need this if the focused track is a MIDI track
        // it doesn't hurt but it's extra work gathering things that won't be displayed
        // revisit
        refreshTrack(state, tstate, view, tview, active);

        // detect whether the active track changed from the last refresh
        // UI componenents that are sensitive to the active track MUST
        // test this, because the refresh flags within each track view
        // may not be different

        // no, the focusedTrack controls this now
        if (active) {
            // if this moved since the last time, then it moved due to GlobalReset
            // or an old script, or something else that forced track selection not
            // in the UI's control
            // the expectation is that this change focus, certainly if you are currently
            // over an audio track, if currently over a MIDI track it's less clear, might
            // want those to operate independently
            if (view->activeAudioTrack != i) {
                if (view->focusedTrack != i) {
                    // we're warping, trace just to see if it happens as I expect
                    //Trace(2, "MobiusViewer: Where the Mobius Track leads, I follow");
                    view->focusedTrack = i;
                }
                // else, Mobius is following focus, which is normal since it takes awhile
                // for the Kernel to catch up
                view->activeAudioTrack = i;
            }
        }
        
#if 0        
        if (active) {
            
            // cache a pointer to the active track
            view->track = tview;
            
            if (view->activeTrack != i) {
                view->trackChanged = true;
                view->activeTrack = i;
            }
        }
#endif
        
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
    refreshSync(tstate, tview);
    refreshMode(tstate, tview);
    
    MobiusLoopState* lstate = &(tstate->loops[tstate->activeLoop]);
    refreshActiveLoop(tstate, lstate, active, tview);
}

/**
 * Track name
 * 
 * These are not in MobiusTrackState, they are pulled from the active Setup.
 * These can't be changed with actions, though that might be interesting for
 * more dynamic names.
 */
void MobiusViewer::refreshTrackName(MobiusState* state, MobiusTrackState* tstate,
                                    MobiusView* mview, MobiusViewTrack* tview)
{
    if (mview->setupChanged) {

        tview->name = "";
        MobiusConfig* config = supervisor->getMobiusConfig();
        Setup* setup = config->getSetup(state->setupOrdinal);
        if (setup != nullptr) {
            // note that MobiusTrackState has an inconsistent use of "number"
            // that usually means 1 based but here it is the "raw number" which is
            // the zero based index, sad
            SetupTrack* st = setup->getTrack(tstate->number);
            if (st != nullptr)
              tview->name = juce::String(st->getName());
        }
        tview->refreshName = true;
    }
}

/**
 * Refresh various simple properties of each track that are not related to
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
 * Refresh things related to the sync source for a track
 *
 * Tempo will be shown if it is non-zero, this applies to both slave
 * sync and master sync.
 *
 * Beats and bars have only been shown if the syncSource is SYNC_MIDI or SYNC_HOST
 * Old code only showed bars if syncUnit was SYNC_UNIT_BAR but now we always do both.
 * 
 */
void MobiusViewer::refreshSync(MobiusTrackState* tstate, MobiusViewTrack* tview)
{
    tview->syncTempo = tstate->tempo;
    tview->syncBeat = tstate->beat;
    tview->syncBar = tstate->bar;
    
    // whether we pay attention to those or not depends on the syncSource
    SyncSource src = tstate->syncSource;
    tview->syncShowBeat = (src == SYNC_MIDI || src == SYNC_HOST);
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
        // should do others this way, let the view defined what to display
        // so the UI components don't have to keep a copy
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
 */
void MobiusViewer::refreshInactiveLoops(MobiusTrackState* tstate, MobiusViewTrack* tview)
{
    tview->loopCount = tstate->loopCount;

    for (int i = 0 ; i < tstate->loopCount ; i++) {
        MobiusLoopState* lstate = &(tstate->loops[i]);
        // grow the loop views as necessary
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
    if (tstate->activeLoop != tview->activeLoop) {
        tview->activeLoop = tstate->activeLoop;
        tview->loopChanged = true;
    }
    
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
    if (tview->nextLoopNumber != lstate->nextLoop) {
        tview->nextLoopNumber = lstate->nextLoop;
        tview->refreshSwitch = true;
    }
    if (tview->returnLoopNumber != lstate->returnLoop) {
        tview->returnLoopNumber = lstate->returnLoop;
        tview->refreshSwitch = true;
    }

    // this flag is set after some form of loop loading happens
    // that can change the sizes of inactive loops,
    // there might be other uses for this in the  future, if so
    // change the name to be more generic, like oh say, needsRefresh
    if (tstate->needsRefresh) {
        tview->refreshLoopContent = true;
        // this is "latching" and we are required to clear it when
        // the UI is prepared to deal with it
        tstate->needsRefresh = false;
    }

    if (activeTrack)
      refreshMinorModes(tstate, lstate, tview);

    // beaters
    if (activeTrack) {
        tview->beatSubcycle = lstate->beatSubCycle;
        tview->beatCycle = lstate->beatCycle;
        tview->beatLoop = lstate->beatLoop;

        // these are "latching" meaning they will remain
        // set until the UI turned them off, don't need this any more
        // was an abandoned attempt to capture them from the audio thread
        lstate->beatSubCycle = false;
        lstate->beatCycle = false;
        lstate->beatLoop = false;
    }
    
    // various
    tview->windowOffset = lstate->windowOffset;
    tview->windowHistoryFrames = lstate->historyFrames;

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

    // had some difficulty using strcmp with the toUTF8 result
    // without first brining it to a const char*, might have been
    // a dream, but if you change this watch for false positives
    const char* newMode = mode->getName();
    const char* oldMode = tview->mode.toUTF8();
    int diff = strcmp(oldMode, newMode);
    if (diff) {
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
 * If you ever need to get subcycles for all tracks, the scope in the Query
 * will need to be changed to include the track number.
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
 * the MobiusState layer model is insanely complicated, thinking that
 * there needed to be a model for each layer, actually don't need anything
 * but checkpoint flags.  Sizes might be nice, bit a pita to maintain.
 */
void MobiusViewer::refreshLayers(MobiusLoopState* lstate, MobiusViewTrack* tview)
{
    // trigger refresh if the layer count changes, or the active layer changes
    int newCount = lstate->layerCount + lstate->lostLayers + lstate->redoCount + lstate->lostRedo;
    int newActive = (lstate->layerCount + lstate->lostLayers) - 1;

    if (newCount != tview->layerCount) {
        tview->layerCount = newCount;
        tview->refreshLayers = true;
    }

    if (newActive != tview->activeLayer) {
        tview->activeLayer = newActive;
        tview->refreshLayers = true;
    }

    // checkpoint detection could be better but it's annoying due to the rare but
    // theoretically unbounded number of them
    // a juce::Bigint would be handy here
    // note that if there was a checkpoint in a "lost" layer we won't be able to show that
    int newChecks = 0;
    for (int i = 0 ; i < lstate->layerCount ; i++) {
        MobiusLayerState* ystate = &(lstate->layers[i]);
        if (ystate->checkpoint)
          newChecks++;
    }

    // until we can be smart about detecting checkpoint changes in each layer
    // just trigger refresh if the number of them changes, the user will almost always
    // be adding new checkpoints, or clearing the checkpoint in the active layer
    // what this doesn't detect is pairs: clearing a checkpoint in an old layer AND
    // setting one in a different layer.  I don't think you can even do that
    int oldChecks = tview->checkpoints.size();
    if (newChecks != oldChecks) {

        tview->checkpoints.clear();
        
        for (int i = 0 ; i < lstate->layerCount ; i++) {
            MobiusLayerState* ystate = &(lstate->layers[i]);
            if (ystate->checkpoint) {
                // the layer number here is the index of this layer in the logical layer
                // view that includes the "lost" layers from the state
                int layerIndex = i + lstate->lostLayers;
                tview->checkpoints.add(layerIndex);
            }
        }

        tview->refreshLayers = true;
    }

    // !! what about redo layers?  you can have checkpoints in those too
    // visualizing those aren't important UNLESS there is a "redo to checkpoint"
    // function, which I think there is...
}

/**
 * Events
 *
 * Difference detection for events is complex because we're dealing with
 * lists of objects.  Do the difference detection at the same time as the refresh.
 * Not trying to be smart about reusing event views and modifying them in place.
 * If anything changes about events, rebuild the entire event view.  There won't be
 * many of these and add/remove is relatively rare.
 */
void MobiusViewer::refreshEvents(MobiusLoopState* lstate, MobiusViewTrack* tview)
{
    int newCount = lstate->eventCount;
    int oldCount = tview->events.size();

    if (newCount != oldCount)
      tview->refreshEvents = true;
    else {
        // counts didn't change but the contents may have
        for (int i = 0 ; i < lstate->eventCount ; i++) {
            MobiusEventState* estate = &(lstate->events[i]);
            MobiusViewEvent* ve = tview->events[i];
        
            const char* newName = nullptr;
            if (estate->type != nullptr)
              newName = estate->type->getName();
            else
              newName = "???";

            // LoopMeter will display both the event type name and the argument
            // number for things like "LoopSwitch 2" so when doing name
            // comparisons, use strncmp to only compare the name without the argument
            
            // more weirdness around the lifespan of toUTF8()
            const char* oldName = ve->name.toUTF8();
            if (strncmp(oldName, newName, strlen(newName)) ||
                ve->argument != estate->argument ||
                ve->frame != estate->frame ||
                ve->pending != estate->pending) {

                tview->refreshEvents = true;
                break;
            }
        }
    }
    
    // if after all that we detected a difference, rebuild the event view
    if (tview->refreshEvents) {
        tview->events.clear();

        for (int i = 0 ; i < lstate->eventCount ; i++) {
            MobiusEventState* estate = &(lstate->events[i]);
            MobiusViewEvent* ve = new MobiusViewEvent();
            tview->events.add(ve);

            // sigh, repeat this little dance
            const char* newName = "???";
            if (estate->type != nullptr) newName = estate->type->getName();

            ve->name = juce::String(newName);
            // the argument is only visible if it is non-zero
            if (ve->argument > 0)
              ve->name += " " + juce::String(ve->argument);
            
            ve->frame = estate->frame;
            ve->pending = estate->pending;
            ve->argument = estate->argument;
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

    // not really a minor mode but convenitnt for some things
    tview->anySpeed = lstate->speed;
    tview->anyPitch = lstate->pitch;
    
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
    if (lstate->historyFrames != tview->windowHistoryFrames) {
        tview->windowHistoryFrames = lstate->historyFrames;
        refresh = true;
    }
        
    tview->refreshMinorModes = refresh;

    if (refresh)
      assembleMinorModes(tview);
}

/**
 * As a convenience for the MinorModesElemenet, assemble the value to display
 * since we have all the information here, and don't want to duplicate all these
 * flags in the element.
 */
void MobiusViewer::assembleMinorModes(MobiusViewTrack* tview)
{
    tview->minorModes.clear();

    if (tview->overdub) addMinorMode(tview, "Overdub");
    if (tview->mute) addMinorMode(tview, "Mute");
    if (tview->reverse) addMinorMode(tview, "Reverse");
    
    if (tview->speedOctave != 0) addMinorMode(tview, "SpeedOct", tview->speedOctave);
    if (tview->speedStep != 0) {
        // factor out the toggle since they
        // are percived at different things
        int step = tview->speedStep - tview->speedToggle;
        if (step != 0)
          addMinorMode(tview, "SpeedStep", step);
    }
    if (tview->speedToggle != 0) addMinorMode(tview, "SpeedToggle", tview->speedToggle);
    // This can also be a knob so we don't need
    // this but I'm not sure people want to waste
    // space for a knob that's too fine grained
    // to use from the UI anyway.
    if (tview->speedBend != 0) addMinorMode(tview, "SpeedBend", tview->speedBend);
    
    if (tview->pitchOctave != 0) addMinorMode(tview, "PitchOctave", tview->pitchOctave);
    if (tview->pitchStep != 0) addMinorMode(tview, "PitchStep", tview->pitchStep);
    if (tview->pitchBend != 0) addMinorMode(tview, "PitchBend", tview->pitchBend);
    
    if (tview->timeStretch != 0) addMinorMode(tview, "TimeStretch", tview->timeStretch);

    // forget why I had the combo here, and why they're a mutex
    if (tview->trackSyncMaster && tview->outSyncMaster) {
        addMinorMode(tview, "Sync Master");
    }
    else if (tview->trackSyncMaster) {
        addMinorMode(tview, "Track Sync Master");
    }
    else if (tview->outSyncMaster) {
        addMinorMode(tview, "MIDI Sync Master");
    }
    
    // the state flag has been "recording" but the mode was displayed
    // as "Capture" so this must have only been used for Capture/Bounce
    if (tview->recording) addMinorMode(tview, "Capture");

    // this would be better as something in the track strip like DAWs do
    if (tview->solo) addMinorMode(tview, "Solo");

    // this is a weird one, it will be set during Solo too...
    if (tview->globalMute && !tview->solo) addMinorMode(tview, "Global Mute");

    if (tview->globalPause) addMinorMode(tview, "Global Pause");
    
    // this is "loop window" mode
    if (tview->windowOffset > 0) addMinorMode(tview, "Windowing");

    // this is what the UI wants to display at the moment
    // don't need both but I'd like to leave it open to display them
    // independently
    tview->minorModesString = tview->minorModes.joinIntoString(" ");
}

/**
 * Helper for minor mode gatherer that deals with the changes in string models.
 */
void MobiusViewer::addMinorMode(MobiusViewTrack* tview, const char* mode)
{
    tview->minorModes.add(juce::String(mode));
}

void MobiusViewer::addMinorMode(MobiusViewTrack* tview, const char* mode, int arg)
{
    tview->minorModes.add(juce::String(mode) + " " + juce::String(arg));
}
    
//////////////////////////////////////////////////////////////////////
//
// MIDI Tracks
//
//////////////////////////////////////////////////////////////////////

void MobiusViewer::refreshMidiTracks(MobiusInterface* mobius, MobiusView* view)
{
    (void)mobius;
    (void)view;
    //MobiusConfig* config = supervisor->getMobiusConfig();
    //view->midiTracks = config->getMidiTracks();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
