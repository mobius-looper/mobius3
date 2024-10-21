/**
 * NOTE: This is in a transition between OldMobiusState and the new MobiusState
 * that will be shared by both audio and midi tracks.
 *
 * Translation between the old MobiusState and the new MobiusView,
 * plus additions and simpliciations.
 *
 * Most of the information is contained in the MobiusViewTrack which drives the display of both
 * the main Status Area in the center, and the list of Track Strips at the bottom.  The Status Area
 * is able to show everything about the track, the Track Strips will have a subset.  This distinction
 * is a minor optimization that should not be assumed to last forever.
 *
 * The refrsh process is assembling the view from several sources.  Most of it comes from MobiusState
 * which was the original way to communicate state from the engine to the UI.  Some is pulled
 * from configuration objects like Setup and GroupDefinition based on references in MoibusState.
 * And some is made by querying the engine for a few real-time parameter values.
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
 * MIDI tracks are presented through the view the same as audio tracks.  The UI should mostly
 * not care what type of track this is.
 *
 * Tracks can be added or removed by editing the session.  Because there is a lag between
 * sending the session down to the kernel and the updated track configuration, the engine
 * may send back MobiusState and MobiusMidiState results that do not match the session.  Always
 * trust the state objects.
 *
 * Once created a MobiusViewTrack will remain in memory for the duration of the application.
 * If track counts are lowered, they are left behind for possible reuse.  The track view array
 * will grow as necessary to match the engine state.
 *
 */

#include <JuceHeader.h>

#include "../Supervisor.h"

#include "../model/OldMobiusState.h"
#include "../model/MobiusMidiState.h"
#include "../model/UIEventType.h"
#include "../model/ModeDefinition.h"
#include "../model/MobiusConfig.h"
#include "../model/Session.h"

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
 * until a restart, but it will someday so always obey the trackCount returned in the
 * MobiusState.
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
    Session* session = supervisor->getSession();

    view->audioTracks = session->audioTracks;
    if (view->audioTracks == 0) {
        // crashy if we don't have at least one, force it
        view->audioTracks = 1;
    }

    view->midiTracks = session->midiTracks;

    view->totalTracks = view->audioTracks + view->midiTracks;

    // flesh these out ahead of time, they can grow if configuration is changed
    // but start with enough for the current session
    // whether these are midi or not is set during refresh
    for (int i = 0 ; i < view->totalTracks ; i++) {
        MobiusViewTrack* vt = new MobiusViewTrack();
        vt->index = i;
        view->tracks.add(vt);
    }

    // always start on the first one
    // this may conflict with the Setup on the first refresh
    view->focusedTrack = 0;
    view->track = view->tracks[0];
}

/**
 * Reconfigure the view after changing the track counts.
 * This has unfortunate race conditions with the kernel since it won't
 * reconfigure itself until the next audio interrupt.  If you change
 * the view then hit a refresh cycle before kernel had a chance to adapt
 * there will be a mismatch between the view and the state objects returned
 * by the engine.  This actually doesn't matter much to the display, it just
 * may cause a little flicker as the tracks change out from under it.
 *
 * The only thing this needs to do is move the focused track, if the track
 * under it was taken away.
 */
void MobiusViewer::configure(MobiusView* view)
{
    Session* session = supervisor->getSession();
    
    if (view->audioTracks != session->audioTracks) {
        Trace(1, "MobiusViewer: Audio track counts changed, this might be a problem");
    }
    
    view->audioTracks = session->audioTracks;
    if (view->audioTracks == 0) {
        // crashy if we don't have at least one, force it
        view->audioTracks = 1;
    }

    view->midiTracks = session->midiTracks;
    view->totalTracks = view->audioTracks + view->midiTracks;

    // grow this when necessary, don't bother with shrinking it
    for (int i = view->tracks.size() ; i < view->totalTracks ; i++) {
        MobiusViewTrack* vt = new MobiusViewTrack();
        vt->index = i;
        view->tracks.add(vt);
    }

    if (view->focusedTrack >= view->totalTracks) {
        // go to the highest or the first?
        view->focusedTrack = view->totalTracks - 1;
        view->track = view->tracks[view->focusedTrack];
    }
}

/**
 * The root of the periodic full refresh.
 *
 * This is expected to be called once every 1/10th second by the
 * maintenance thread. It does not update the UI, it only refreshes
 * the model and sets various flags when something that is more complex
 * changes so the UI can optimize out repaints when nothing is changing.
 */
void MobiusViewer::refresh(MobiusInterface* mobius, OldMobiusState* state, MobiusView* view)
{
    (void)mobius;

    if (state->trackCount != view->audioTracks) {
        Trace(2, "MobiusViewer: Adjusting audio tracks to %d", state->trackCount);
        view->audioTracks = state->trackCount;
    }
    
    // Counter needs this
    view->sampleRate = supervisor->getSampleRate();

    // move the track view to the one that has focus
    // !! now that tracks can be higher than the configured number to use, may
    // need to constrain focus here?
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


    // detect when the selected track changes, this be driven by the state object
    // for audio tracks, but when switching between audio and midi, or within midi
    // we have to detect that at the root
    if (view->lastFocusedTrack != view->focusedTrack) {
        view->trackChanged = true;
        view->lastFocusedTrack = view->focusedTrack;
    }

    refreshAudioTracks(mobius, state, view);

    // MIDI Tracks are glued onto the end of the audio tracks
    refreshMidiTracks(mobius, view);


    // so the display elements don't have to test for view->trackChanged
    // in addition to the element specific refresh flags, if at the end of refresh
    // trackChanged is set, force all the secondary flags on
    if (view->trackChanged)
      forceRefresh(view);
    
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

void MobiusViewer::refreshAudioTracks(MobiusInterface* mobius, OldMobiusState* state, MobiusView* view)
{
    (void)mobius;

    // !! OldMobiusState has no way to tell us how many tracks are in the state
    // have to trust the view, this is bad
    for (int i = 0 ; i < state->trackCount ; i++) {
        OldMobiusTrackState* tstate = &(state->tracks[i]);

        MobiusViewTrack* tview = nullptr;
        if (i < view->tracks.size())
          tview = view->tracks[i];
        else {
            // something is messed up, there should always be one at this index
            Trace(1, "MobiusViewer: View track array is hosed, and so are you");
            break;
        }

        // clear this incase it was midi in a past life
        tview->midi = false;

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
                    view->trackChanged = true;
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
void MobiusViewer::refreshTrack(OldMobiusState* state, OldMobiusTrackState* tstate,
                                MobiusView* mview, MobiusViewTrack* tview,
                                bool active)
{
    refreshTrackName(state, tstate, mview, tview);
    refreshInactiveLoops(tstate, tview);
    refreshTrackProperties(tstate, tview);
    refreshSync(tstate, tview);
    refreshMode(tstate, tview);
    
    OldMobiusLoopState* lstate = &(tstate->loops[tstate->activeLoop]);
    refreshActiveLoop(tstate, lstate, active, tview);
}

/**
 * Track name
 * 
 * These are not in OldMobiusTrackState, they are pulled from the active Setup.
 * These can't be changed with actions, though that might be interesting for
 * more dynamic names.
 */
void MobiusViewer::refreshTrackName(OldMobiusState* state, OldMobiusTrackState* tstate,
                                    MobiusView* mview, MobiusViewTrack* tview)
{
    if (mview->setupChanged) {

        tview->name = "";
        MobiusConfig* config = supervisor->getMobiusConfig();
        Setup* setup = config->getSetup(state->setupOrdinal);
        if (setup != nullptr) {
            // note that OldMobiusTrackState has an inconsistent use of "number"
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
void MobiusViewer::refreshTrackProperties(OldMobiusTrackState* tstate, MobiusViewTrack* tview)
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
void MobiusViewer::refreshSync(OldMobiusTrackState* tstate, MobiusViewTrack* tview)
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
 * is 1 based in OldMobiusState with 0 meaning that the track is not assigned
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
void MobiusViewer::refreshTrackGroups(OldMobiusTrackState* tstate,  MobiusViewTrack* tview)
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
 * with OldMobiusLoopState during 3 development.
 */
void MobiusViewer::refreshInactiveLoops(OldMobiusTrackState* tstate, MobiusViewTrack* tview)
{
    tview->loopCount = tstate->loopCount;

    for (int i = 0 ; i < tstate->loopCount ; i++) {
        OldMobiusLoopState* lstate = &(tstate->loops[i]);
        // grow the loop views as necessary
        MobiusViewLoop* lview;
        if (i < tview->loops.size())
          lview = tview->loops[i];
        else {
            lview = new MobiusViewLoop();
            tview->loops.add(lview);
        }

        // there isn't much to say other than whether it is empty or not
        lview->frames = (int)(lstate->frames);

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
void MobiusViewer::refreshActiveLoop(OldMobiusTrackState* tstate, OldMobiusLoopState* lstate,
                                     bool activeTrack, MobiusViewTrack* tview)
{
    if (tstate->activeLoop != tview->activeLoop) {
        tview->activeLoop = tstate->activeLoop;
        tview->loopChanged = true;
    }
    
    // things important for both the main display and the track strips
    tview->recording = lstate->recording;
    tview->modified = lstate->modified;
    tview->pause = lstate->paused;
    tview->frames = (int)(lstate->frames);
    tview->frame = (int)(lstate->frame);

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
    tview->windowOffset = (int)(lstate->windowOffset);
    tview->windowHistoryFrames = (int)(lstate->historyFrames);

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
void MobiusViewer::refreshMode(OldMobiusTrackState* tstate, MobiusViewTrack* tview)
{
    OldMobiusLoopState* loop = &(tstate->loops[tstate->activeLoop]);
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
    if (supervisor->doQuery(&subcyclesQuery)) {

        // todo: view indexes are assumed to correspond directly to track
        // numbers. Once we support track reorder this will need to change
        // and should be using track ids "a3", "m2", etc.
        subcyclesQuery.scope = tview->index + 1;
        subcycles = subcyclesQuery.value;
    }

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
 * the OldMobiusState layer model is insanely complicated, thinking that
 * there needed to be a model for each layer, actually don't need anything
 * but checkpoint flags.  Sizes might be nice, bit a pita to maintain.
 */
void MobiusViewer::refreshLayers(OldMobiusLoopState* lstate, MobiusViewTrack* tview)
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
        OldMobiusLayerState* ystate = &(lstate->layers[i]);
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
            OldMobiusLayerState* ystate = &(lstate->layers[i]);
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
void MobiusViewer::refreshEvents(OldMobiusLoopState* lstate, MobiusViewTrack* tview)
{
    int newCount = lstate->eventCount;
    int oldCount = tview->events.size();

    if (newCount != oldCount)
      tview->refreshEvents = true;
    else {
        // counts didn't change but the contents may have
        for (int i = 0 ; i < lstate->eventCount ; i++) {
            OldMobiusEventState* estate = &(lstate->events[i]);
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
            OldMobiusEventState* estate = &(lstate->events[i]);
            MobiusViewEvent* ve = new MobiusViewEvent();
            tview->events.add(ve);

            // sigh, repeat this little dance
            const char* newName = "???";
            if (estate->type != nullptr) newName = estate->type->getName();

            ve->name = juce::String(newName);
            // the argument is only visible if it is non-zero
            if (estate->argument > 0)
              ve->name += " " + juce::String(estate->argument);
            
            ve->frame = (int)(estate->frame);
            ve->pending = (int)(estate->pending);
            ve->argument = (int)(estate->argument);
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
void MobiusViewer::refreshMinorModes(OldMobiusTrackState* tstate, OldMobiusLoopState* lstate,
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
        tview->windowOffset = (int)(lstate->windowOffset);
        refresh = true;
    }
    if (lstate->historyFrames != tview->windowHistoryFrames) {
        tview->windowHistoryFrames = (int)(lstate->historyFrames);
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
    // no, recording means any type of recording which is used to color things red
    // capture/Bounce is something else, need a different flag for that
    //if (tview->recording) addMinorMode(tview, "Capture");

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
// While the class is named MobiusMidiState it is really a new model
// that will eventually be used for both audio and MIDI tracks.  
//
//////////////////////////////////////////////////////////////////////

void MobiusViewer::refreshMidiTracks(MobiusInterface* mobius, MobiusView* view)
{
    MobiusMidiState* state = mobius->getMidiState();

    if (view->midiTracks != state->activeTracks) {
        Trace(2, "MobiusViewer: Adjusting MIDI track view to %d", state->activeTracks);
        view->midiTracks = state->activeTracks;
    }

    // add new ones
    int required = view->audioTracks + view->midiTracks;
    while (required > view->tracks.size()) {
        MobiusViewTrack *vt = new MobiusViewTrack();
        vt->index = view->tracks.size();
        view->tracks.add(vt);
    }
    
    for (int i = 0 ; i < state->activeTracks ; i++) {
        MobiusMidiState::Track* tstate = state->tracks[i];
        int vtrackIndex = view->audioTracks + i;
        if (vtrackIndex < view->tracks.size()) {
            MobiusViewTrack* tview = view->tracks[vtrackIndex];
            refreshMidiTrack(tstate, tview);
        }
        else {
            Trace(1, "MobiusViweer: MIDI view tracks are fucked");
            break;
        }
    }
}

void MobiusViewer::refreshMidiTrack(MobiusMidiState::Track* tstate, MobiusViewTrack* tview)
{
    tview->midi = true;
    tview->loopCount = tstate->loopCount;
    tview->activeLoop = tstate->activeLoop;
    
    tview->frame = tstate->frame;
    // having trouble tracking reset for some reason
    if (tview->frames > 0 && tstate->frames == 0)
      tview->refreshLoopContent = true;
    
    tview->frames = (int)(tstate->frames);
    tview->subcycles = tstate->subcycles;
    tview->subcycle = tstate->subcycle;
    tview->cycles = tstate->cycles;
    tview->cycle = tstate->cycle;

    tview->inputMonitorLevel = tstate->inputMonitorLevel;
    tview->outputMonitorLevel = tstate->outputMonitorLevel;

    tview->inputLevel = tstate->input;
    tview->outputLevel = tstate->output;
    tview->feedback = tstate->feedback;
    tview->pan = tstate->pan;
    
    // fake these up to avoid warnings in LoopMeterElement and LoopStackElement
    if (tview->cycle == 0) tview->cycle = 1;
    if (tview->subcycles == 0) tview->subcycles = 4;

    if (tview->recording != tstate->recording) {
        tview->recording = tstate->recording;
        tview->refreshLoopContent = true;
    }

    if (tview->modified != tstate->modified) {
        tview->modified = tstate->modified;
        tview->refreshLoopContent =  true;
    }
    
    tview->pause = tstate->pause;

    if (tview->nextLoopNumber != tstate->nextLoop) {
        tview->nextLoopNumber = tstate->nextLoop;
        tview->refreshSwitch = true;
    }

    juce::String newMode = MobiusMidiState::getModeName(tstate->mode);
    // MidiTrack does this transformation now too
    if (tstate->mode == MobiusMidiState::ModePlay && tstate->overdub)
      newMode = "Overdub";

    if (newMode != tview->mode) {
        tview->mode = newMode;
        tview->refreshMode = true;
    }

    refreshMidiMinorModes(tstate, tview);

    // inactive loop state, can grow these dynamically
    // note that the MobiusMidiState::Loop array may be larger than the loopCount
    for (int i = tview->loops.size() ; i <= tstate->loopCount ; i++) {
        MobiusViewLoop* vl = new MobiusViewLoop();
        tview->loops.add(vl);
    }

    for (int i = 0 ; i < tstate->loopCount ; i++) {
        MobiusViewLoop* vl = tview->loops[i];
        MobiusMidiState::Loop* lstate = tstate->loops[i];
        if (lstate == nullptr) {
            Trace(1, "MidiViewer: MobiusMidiState loop array too small");
        }
        else {
            vl->frames = (int)(lstate->frames);
        }
    }

    tview->layerCount = tstate->layerCount;
    tview->activeLayer = tstate->activeLayer;
    // checkpoints not implemented yet

    refreshMidiEvents(tstate, tview);
    refreshRegions(tstate, tview);
}

void MobiusViewer::refreshMidiMinorModes(MobiusMidiState::Track* tstate, 
                                         MobiusViewTrack* tview)
{
    bool refresh = false;

    if (tstate->reverse != tview->reverse) {
        tview->reverse =  tstate->reverse;
        refresh = true;
    }

    if (tstate->overdub != tview->overdub) {
        tview->overdub = tstate->overdub;
        refresh = true;
    }

    if (tstate->mute != tview->mute) {
        tview->mute = tstate->mute;
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
    
    if (refresh) {
        assembleMinorModes(tview);
        tview->refreshMinorModes = true;
    }
}
    
void MobiusViewer::refreshMidiEvents(MobiusMidiState::Track* tstate, MobiusViewTrack* tview)
{
    int newCount = tstate->eventCount;
    int oldCount = tview->events.size();

    if (newCount != oldCount)
      tview->refreshEvents = true;
    else {
        // counts didn't change but the contents may have
        for (int i = 0 ; i < tstate->eventCount ; i++) {
            MobiusMidiState::Event* estate = tstate->events[i];
            MobiusViewEvent* ve = tview->events[i];
        
            // LoopMeter will display both the event type name and the argument
            // number for things like "LoopSwitch 2" so when doing name
            // comparisons, use strncmp to only compare the name without the argument
            const char* newName = estate->name.toUTF8();
            
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
    // sweet jesus this is horrible, memory allocations every damn time !?
    if (tview->refreshEvents) {
        tview->events.clear();

        for (int i = 0 ; i < tstate->eventCount ; i++) {
            MobiusMidiState::Event* estate = tstate->events[i];
            MobiusViewEvent* ve = new MobiusViewEvent();
            tview->events.add(ve);
            
            ve->name = estate->name;
            // the argument is only visible if it is non-zero
            if (estate->argument > 0)
              ve->name += " " + juce::String(estate->argument);
            
            ve->frame = estate->frame;
            ve->pending = estate->pending;
            ve->argument = estate->argument;
        }
    }
}

void MobiusViewer::refreshRegions(MobiusMidiState::Track* tstate, MobiusViewTrack* tview)
{
    // yet ANOTHER copy of this
    tview->regions.clearQuick();
    for (int i = 0 ; i < tstate->regions.size() && i < MobiusMidiState::MaxRegions ; i++) {
        MobiusMidiState::Region& src = tstate->regions.getReference(i);
        tview->regions.add(src);
    }
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
void MobiusViewer::refreshSync(MobiusMidiState::Track* tstate, MobiusViewTrack* tview)
{
    tview->syncTempo = tstate->tempo;
    tview->syncBeat = tstate->beat;
    tview->syncBar = tstate->bar;
    
    // whether we pay attention to those or not depends on the syncSource
    SyncSource src = tstate->syncSource;
    tview->syncShowBeat = (src == SYNC_MIDI || src == SYNC_HOST);
}    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
