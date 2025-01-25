/**
 * NOTE: This is in a transition between OldMobiusState and the new SysstemStatMobiusState
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
 * may send back MobiusState and MobiusState results that do not match the session.  Always
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
#include "../model/SystemState.h"
#include "../model/TrackState.h"

#include "../model/UIEventType.h"
#include "../model/ModeDefinition.h"
#include "../model/MobiusConfig.h"
#include "../model/Session.h"

#include "MobiusView.h"
#include "MobiusViewer.h"

//////////////////////////////////////////////////////////////////////
//
// MObiusViewer
//
//////////////////////////////////////////////////////////////////////


MobiusViewer::MobiusViewer(Provider* p)
{
    provider = p;
    
    // initialize the Query we use to dig out the runtime subcycles
    // parameter value
    subcyclesQuery.symbol = p->getSymbols()->intern("subcycles");
}

MobiusViewer::~MobiusViewer()
{
}

/**
 * Initialize the view at startup.
 *
 * Since we reuse track view objects in the array once created, in fringe cases where you are
 * adding and removing tracks of different types this should be doing a better job of initializing
 * track views that become unused, then reused.
 */
void MobiusViewer::initialize(MobiusView* view)
{
    Session* session = provider->getSession();

    view->audioTracks = session->getAudioTracks();
    if (view->audioTracks == 0) {
        // crashy if we don't have at least one, force it
        // !! why  fix this
        Trace(1, "MobiusViewer: Forcing a single audio track, why?");
        view->audioTracks = 1;
    }

    view->midiTracks = session->getMidiTracks();
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
    Session* session = provider->getSession();
    
    if (view->audioTracks != session->getAudioTracks()) {
        Trace(1, "MobiusViewer: Audio track counts changed");
    }
    view->audioTracks = session->getAudioTracks();
    if (view->audioTracks == 0) {
        // crashy if we don't have at least one, force it
        view->audioTracks = 1;
    }

    view->midiTracks = session->getMidiTracks();
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
void MobiusViewer::refresh(SystemState* sysstate, MobiusView* view)
{
    OldMobiusState* state = sysstate->oldState;
    
    if (state != nullptr && state->trackCount != view->audioTracks) {
        Trace(1, "MobiusViewer: Adjusting audio tracks to %d", state->trackCount);
        view->audioTracks = state->trackCount;
    }
    
    // Counter needs this
    view->sampleRate = provider->getSampleRate();

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

    // update: this doesn't mean anything now, there will only be one Setup in
    // core and the ordinal never changes
    if (state != nullptr) {
        if (view->setupOrdinal != state->setupOrdinal) {
            view->setupChanged = true;
            view->setupOrdinal = state->setupOrdinal;
        }
    }

    // detect when the selected track changes, this be driven by the state object
    // for audio tracks, but when switching between audio and midi, or within midi
    // we have to detect that at the root
    if (view->lastFocusedTrack != view->focusedTrack) {
        view->trackChanged = true;
        view->lastFocusedTrack = view->focusedTrack;
    }

    // do a full refresh from the new model
    refreshAllTracks(sysstate, view);

    // temporarily corrrect the view from the old model until the new one
    // is fully tested
    
    if (state != nullptr)
      refreshAudioTracks(state, view);

    // dump the entire sync state over, no need to duplicate
    view->syncState = sysstate->syncState;

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

void MobiusViewer::refreshAudioTracks(OldMobiusState* state, MobiusView* view)
{
    int nextViewTrack = 0;
    
    for (int i = 0 ; i < state->trackCount ; i++) {
        OldMobiusTrackState* tstate = &(state->tracks[i]);

        // the view contains tracks in Session order which will be a mixture
        // of audio and MIDI.  OldMobiusState only contains audio.  Have to search
        // for the target view
        MobiusViewTrack* tview = nullptr;
        while (nextViewTrack < view->tracks.size()) {
            MobiusViewTrack* maybe = view->tracks[nextViewTrack];
            if (maybe->midi)
              nextViewTrack++;
            else {
                tview = maybe;
                break;
            }
        }

        if (tview == nullptr) {
            Trace(1, "MobiusViewer: Ran out of view tracks looking for audio view");
        }
        else {
            // clear this incase it was midi in a past life
            tview->midi = false;

            // only audio tracks have the concept of an active track
            // this is NOT the same as the view's focused track
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
    (void)state;
    (void)mview;
    //refreshTrackName(state, tstate, mview, tview);
    //refreshInactiveLoops(tstate, tview);
    //refreshTrackProperties(tstate, tview);

    // getting sync from the new model now
    //refreshSync(tstate, tview);
    
    //refreshMode(tstate, tview);
    
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
#if 0
void MobiusViewer::refreshTrackName(OldMobiusState* state, OldMobiusTrackState* tstate,
                                    MobiusView* mview, MobiusViewTrack* tview)
{
    if (mview->setupChanged) {

        tview->name = "";
        MobiusConfig* config = provider->getMobiusConfig();
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
#endif

/**
 * Refresh various simple properties of each track that are not related to
 * a particular loop.
 */
#if 0
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
#endif

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
#if 0
void MobiusViewer::refreshSync(OldMobiusTrackState* tstate, MobiusViewTrack* tview)
{
    tview->syncTempo = tstate->tempo;
    tview->syncBeat = tstate->beat;
    tview->syncBar = tstate->bar;
    
    // whether we pay attention to those or not depends on the syncSource
    OldSyncSource src = tstate->syncSource;
    tview->syncShowBeat = (src == SYNC_MIDI || src == SYNC_HOST);
}    
#endif

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
#if 0
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
        
        MobiusConfig* config = provider->getMobiusConfig();
        
        // ignore if out of range
        if (newNumber > 0 && newNumber <= config->groups.size()) {
            GroupDefinition* group = config->groups[newNumber - 1];
            tview->groupName = group->name;
            tview->groupColor = group->color;
        }

        tview->refreshGroup = true;
    }
}
#endif

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
#if 0
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
#endif

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

    //if (activeTrack)
    //refreshMinorModes(tstate, lstate, tview);

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

    //if (activeTrack)
    //refreshEvents(lstate, tview);
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
#if 0
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
#endif

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
    if (provider->doQuery(&subcyclesQuery)) {

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
#if 0
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
            MobiusViewEvent& ve = tview->events.getReference(i);
        
            const char* newName = nullptr;
            if (estate->type != nullptr)
              newName = estate->type->getName();
            else
              newName = "???";

            // LoopMeter will display both the event type name and the argument
            // number for things like "LoopSwitch 2" so when doing name
            // comparisons, use strncmp to only compare the name without the argument
            
            // more weirdness around the lifespan of toUTF8()
            const char* oldName = ve.name.toUTF8();
            if (strncmp(oldName, newName, strlen(newName)) ||
                ve.argument != estate->argument ||
                ve.frame != estate->frame ||
                ve.pending != estate->pending) {

                tview->refreshEvents = true;
                break;
            }
        }
    }
    
    // if after all that we detected a difference, rebuild the event view
    if (tview->refreshEvents) {
        tview->events.clearQuick();

        for (int i = 0 ; i < lstate->eventCount ; i++) {
            OldMobiusEventState* estate = &(lstate->events[i]);
            MobiusViewEvent ve;

            // sigh, repeat this little dance
            const char* newName = "???";
            if (estate->type != nullptr) newName = estate->type->getName();

            ve.name = juce::String(newName);
            // the argument is only visible if it is non-zero
            if (estate->argument > 0)
              ve.name += " " + juce::String(estate->argument);
            
            ve.frame = (int)(estate->frame);
            ve.pending = (int)(estate->pending);
            ve.argument = (int)(estate->argument);
            
            tview->events.add(ve);
        }
    }
}
#endif

/**
 * Minor Modes
 *
 * There are a LOT of minor modes, but the display element displays all of them
 * at once in a list.  So do refresh detection on the entire collection.
 *
 * Go ahead and assemble the value too, though that should be up to the element?
 */
#if 0
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

    if (tstate->outSyncMaster != tview->transportMaster) {
        tview->transportMaster = tstate->outSyncMaster;
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
#endif

//////////////////////////////////////////////////////////////////////
//
// New State Model
//
//////////////////////////////////////////////////////////////////////

/**
 * This is what we should be doing for all tracks as soon as core
 * refreshes the new TrackState model properly.
 */
void MobiusViewer::refreshAllTracks(SystemState* state, MobiusView* view)
{
    // state changes along with the Session, but the view can lag
    // if things got bigger, grow
    if (view->midiTracks != state->midiTracks) {
        Trace(2, "MobiusViewer: Adjusting MIDI track view to %d", state->midiTracks);
        view->midiTracks = state->midiTracks;
    }

    if (view->audioTracks != state->audioTracks) {
        Trace(2, "MobiusViewer: Adjusting audio tracks to %d", state->audioTracks);
        view->audioTracks = state->audioTracks;
    }
    
    // add new ones
    int required = view->audioTracks + view->midiTracks;
    while (required > view->tracks.size()) {
        MobiusViewTrack *vt = new MobiusViewTrack();
        vt->index = view->tracks.size();
        view->tracks.add(vt);
    }

    for (int i = 0 ; i < required ; i++) {

        // sanity check before we start indexing
        // neither of these should happen
        if (i >= state->tracks.size()) {
            Trace(1, "MobiusViewer: State track index overflow");
        }
        else if (i >= view->tracks.size()) {
            Trace(1, "MobiusViewer: View track index overflow");
        }
        else {
            TrackState* tstate = state->tracks[i];
            MobiusViewTrack* tview = view->tracks[i];

            // make sure the state and the view are both numbered properly
            // shouldn't need this but some of the code around sync needs these
            // to be accurate
            int number = i+1;
            if (tstate->number != number) {
                Trace(1, "MobiusViewer: Correcting TrackState number %d/%d",
                      tstate->number, number);
                tstate->number = number;
            }
            
            if (tview->index != number - 1) {
                Trace(1, "MobiusViewer: Incorrect MobiusView::Track index %d for number %d",
                      tview->index, number);
                // don't repair this, this is more sensitive
            }
            
            refreshTrack(state, tstate, view, tview);
        }
    }

    if (view->focusedTrack < 0 || view->focusedTrack >= view->tracks.size()) {
        Trace(1, "MobiusViewer: view->focused track out of range");
    }
    else {
        MobiusViewTrack* tview = view->tracks[view->focusedTrack];
        if (tview == nullptr) {
            Trace(1, "MobiusViewer: Track index overflow");
        }
        else {
            FocusedTrackState* tstate = &(state->focusedState);
            refreshRegions(tstate, tview);
            refreshEvents(tstate, tview);
            refreshLayers(tstate, tview);
        }
    }
}

/**
 * This currently refreshes only the state for MIDI tracks, but the model
 * in both the SystemState and MobiusView is generic and will eventually
 * be used for all track types.
 *
 * The SystemState will be fleshed out with a TrackState for all tracks, but
 * only the MIDI tracks will be filled.  Tracks are identified by internal
 * number.  
 */
#if 0
void MobiusViewer::refreshMidiTracks(SystemState* state, MobiusView* view)
{
    // state changes along with the Session, but the view can lag
    // if things got bigger, grow
    if (view->midiTracks != state->midiTracks) {
        Trace(2, "MobiusViewer: Adjusting MIDI track view to %d", state->midiTracks);
        view->midiTracks = state->midiTracks;
    }

    // add new ones
    int required = view->audioTracks + view->midiTracks;
    while (required > view->tracks.size()) {
        MobiusViewTrack *vt = new MobiusViewTrack();
        vt->index = view->tracks.size();
        view->tracks.add(vt);
    }

    // jump to the MIDI tracks
    for (int i = 0 ; i < state->midiTracks ; i++) {
        
        int trackIndex = view->audioTracks + i;

        // sanity check before we start indexing
        // neither of these should happen
        if (trackIndex >= state->tracks.size()) {
            Trace(1, "MobiusViewer: Track index overflow");
        }
        else if (trackIndex >= view->tracks.size()) {
            Trace(1, "MobiusViewer: Track index overflow");
        }
        else {
            TrackState* tstate = state->tracks[trackIndex];
            MobiusViewTrack* tview = view->tracks[trackIndex];

            refreshTrack(state, tstate, tview);
        }
    }

    if (view->focusedTrack > 0) {
        MobiusViewTrack* tview = view->tracks[view->focusedTrack];
        if (tview == nullptr) {
            Trace(1, "MobiusViewer: Track index overflow");
        }
        else {
            FocusedTrackState* tstate = &(state->focusedState);
            refreshRegions(tstate, tview);
            refreshEvents(tstate, tview);
            refreshLayers(tstate, tview);
        }
    }
}
#endif

/**
 * Refresh a track view from the new TrackState model.
 */ 
void MobiusViewer::refreshTrack(SystemState* state, TrackState* tstate,
                                MobiusView* mview, MobiusViewTrack* tview)
{
    tview->midi = tstate->midi;
    tview->loopCount = tstate->loopCount;
    if (tstate->activeLoop != tview->activeLoop) {
        tview->activeLoop = tstate->activeLoop;
        // todo: the CounterElement watches this instead of the activeLoop number
        // for some reason, it's the only thing triggered by loopChanged
        // so unless there is more here we don't need the flag, just test the number
        tview->loopChanged = true;
    }
    
    tview->frame = tstate->frame;
    // having trouble tracking reset for some reason
    if (tview->frames > 0 && tstate->frames == 0)
      tview->refreshLoopContent = true;

    // special flag set after file loading, jesus, it might just be easier
    // and more reliable to test the lengths of every loop
    if (tstate->refreshLoopContent) {
        tview->refreshLoopContent = true;
        // this is "latching" and we are required to clear it when
        // the UI is prepared to deal with it
        tstate->refreshLoopContent = false;
    }
    
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
    tview->focused = tstate->focus;

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
    if (tview->returnLoopNumber != tstate->returnLoop) {
        tview->returnLoopNumber = tstate->returnLoop;
        tview->refreshSwitch = true;
    }

    juce::String newMode = TrackState::getModeName(tstate->mode);
    // MidiTrack does this transformation now too
    if (tstate->mode == TrackState::ModePlay && tstate->overdub)
      newMode = "Overdub";

    // !! OldMobiusState did these mode transformations
    // if (tstate->globalMute) mode = UIGlobalMuteMode;
    // if (loop->paused) mode = UIPauseMode;
    // if (tstate->globalPause) mode = UIGlobalPauseMode;
    //
    // GlobalPause seems to have been broken for some time
    // Track::mGlobalMute is set by the Mute function
    // Solo is wound up in this too
    //
    // These need to be maintained above Track, MidiTrack isn't
    // setting this
    if (tstate->globalMute)
      newMode = "Global Mute";

    if (tstate->pause)
      newMode = "Pause";

    if (newMode != tview->mode) {
        tview->mode = newMode;
        tview->refreshMode = true;
    }

    refreshMinorModes(state, tstate, tview);

    // inactive loop state, can grow these dynamically
    // note that the TrackState::Loop array may be larger than the loopCount
    for (int i = tview->loops.size() ; i <= tstate->loopCount ; i++) {
        MobiusViewLoop* vl = new MobiusViewLoop();
        tview->loops.add(vl);
    }

    for (int i = 0 ; i < tstate->loopCount && i < TrackState::MaxLoops ; i++) {
        MobiusViewLoop* vl = tview->loops[i];
        TrackState::Loop& lstate = tstate->loops.getReference(i);
        vl->frames = (int)(lstate.frames);
    }

    tview->layerCount = tstate->layerCount;
    tview->activeLayer = tstate->activeLayer;
    // checkpoints not implemented yet

    refreshSync(state, tstate, tview);
    refreshTrackGroups(tstate, tview);
    refreshTrackName(state, tstate, mview, tview);
}

/**
 * This is still built around the Setup which needs to transition
 * to the Session someday.
 * It relies on MobiusView having the setupChanged flag set
 * and needs the setupOrdinal number captured from Mobius.
 *
 * update: Setups are gone, the track names come directly from the Session
 */
void MobiusViewer::refreshTrackName(SystemState* state, TrackState* tstate,
                                    MobiusView* mview, MobiusViewTrack* tview)
{
#if 0    
    if (mview->setupChanged) {

        tview->name = "";
        MobiusConfig* config = provider->getMobiusConfig();
        Setup* setup = config->getSetup(state->setupOrdinal);
        if (setup != nullptr) {
            SetupTrack* st = setup->getTrack(tstate->number - 1);
            if (st != nullptr)
              tview->name = juce::String(st->getName());
        }
        tview->refreshName = true;
    }
#endif
    (void)mview;
    (void)state;
    tview->name = "";
    Session* session = provider->getSession();
    Session::Track* track = session->getTrackByNumber(tstate->number);
    if (track == nullptr)
      Trace(1, "MobiusViewer: Track number out of range in session %d", tstate->number);
    else
      tview->name = track->name;
}

void MobiusViewer::refreshMinorModes(SystemState* state, TrackState* tstate, MobiusViewTrack* tview)
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

    if (tstate->solo != tview->solo) {
        tview->solo = tstate->solo;
        refresh = true;
    }
    
    bool isTransportMaster = (state->syncState.transportMaster == tstate->number);
    bool isTrackMaster = (state->syncState.trackSyncMaster == tstate->number);
    
    if (isTransportMaster != tview->transportMaster) {
        tview->transportMaster = isTransportMaster;
        refresh = true;
    }
	if (isTrackMaster != tview->trackSyncMaster) {
        tview->trackSyncMaster = isTrackMaster;
        refresh = true;
    }

    // not really a minor mode but convenitnt for some things
    tview->anySpeed = tstate->speed;
    tview->anyPitch = tstate->pitch;
    
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

    // loop windowing
    if (tstate->windowOffset != tview->windowOffset) {
        tview->windowOffset = tstate->windowOffset;
        refresh = true;
    }
    if (tstate->historyFrames != tview->windowHistoryFrames) {
        tview->windowHistoryFrames = tstate->historyFrames;
        refresh = true;
    }

    // OldMobiusState viewer included these
    // why the fuck are these here?
#if 0    
    if (tstate->globalMute != tview->globalMute) {
        tview->globalMute = tstate->globalMute;
        refresh = true;
    }
    if (tstate->globalPause != tview->globalPause) {
        tview->globalPause = tstate->globalPause;
        refresh = true;
    }
#endif    
    
    if (refresh) {
        assembleMinorModes(tview);
        tview->refreshMinorModes = true;
    }
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
    if (tview->trackSyncMaster && tview->transportMaster) {
        addMinorMode(tview, "Sync Master");
    }
    else if (tview->trackSyncMaster) {
        addMinorMode(tview, "Track Sync Master");
    }
    else if (tview->transportMaster) {
        addMinorMode(tview, "Transport Master");
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

/**
 * To get events refreshed you're supposed to set tview->refreshEvents.
 * OldMobiusState code did some analysis of the event names to figure this
 * out but it's annoying.  In practice it didn't matter because events are
 * displayed by the LoopMeterElement which fully repains itself whenever the
 * play frame changes, and it normally changes all the time.  What would be missed
 * if you don't set refreshEvents is events that get added while the loop is in Pause.
 *
 * Looking for refreshable differences at this level is ugly, would be better if the
 * engine could keep track of when events are added/removed in each block but that's
 * also error prone.
 *
 * todo: LoopMeterElement is is one of the worst repainters, need to break this up and
 * keep the event list seperate at which point this flag becomes important.
 */
void MobiusViewer::refreshEvents(FocusedTrackState* tstate, MobiusViewTrack* tview)
{
    int newCount = tstate->eventCount;
    int oldCount = tview->events.size();

    // the easiest thing here is to refresh every time when the count is greater than zero
    // and refresh once when it goes from non-zero to zero
    // most of the time it will do nothing, it will refresh too often once there are
    // events but there aren't usually any events
    if (newCount > 0 || oldCount > 0)
      tview->refreshEvents = true;
    
    tview->events.clearQuick();
    for (int i = 0 ; i < tstate->eventCount ; i++) {
        TrackState::Event& e = tstate->events.getReference(i);
        MobiusViewEvent ve;
        expandEventName(e, ve.name);
        if (e.argument > 0)
          ve.name += " " + juce::String(e.argument);
        ve.frame = e.frame;
        ve.pending = e.pending;
        ve.argument = e.argument;
        tview->events.add(ve);
    }
}

/**
 * This gets converted into an array of ints representing the layer
 * numbers that are checkpoints.
 */
void MobiusViewer::refreshLayers(FocusedTrackState* tstate, MobiusViewTrack* tview)
{
    (void)tstate;
    (void)tview;
}

void MobiusViewer::expandEventName(TrackState::Event& e, juce::String& name)
{
    switch (e.type) {
        case TrackState::EventNone:
            // placeholder for "unspecified" should not be seen
            name = "None";
            break;

        case TrackState::EventUnknown:
            // catch-all event for internal events that don't have mappings
            name = "Unknown";
            break;

        case TrackState::EventAction:
        case TrackState::EventRound: {
            Symbol* s = provider->getSymbols()->getSymbol(e.symbol);
            if (s == nullptr)
              name = "Bad Symbol";
            else
              name = s->name;
            if (e.type == TrackState::EventRound)
              name += " End";
        }
            break;

        case TrackState::EventSwitch:
            name = "Switch";
            break;

        case TrackState::EventReturn:
            name = "Return";
            break;
            
        case TrackState::EventWait:
            name = "Wait";
            break;
            
        case TrackState::EventFollower:
            name = "Follower";
            break;
    }
}

/**
 * These are easier than events because there is no name transformation.
 * The structures can just be copied.
 * These are only returned for the focused track, caller is responsible for
 * restricting that.
 */
void MobiusViewer::refreshRegions(FocusedTrackState* tstate, MobiusViewTrack* tview)
{
    tview->regions.clearQuick();
    for (int i = 0 ; i < tstate->regionCount ; i++) {
        TrackState::Region& src = tstate->regions.getReference(i);
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
 */
void MobiusViewer::refreshSync(SystemState* state, TrackState* tstate, MobiusViewTrack* tview)
{
    (void)state;
    tview->syncSource = tstate->syncSource;
    tview->syncUnit = tstate->syncUnit;
    tview->syncTempo = 0.0f;
    tview->syncBeat = tstate->syncBeat;
    tview->syncBar = tstate->syncBar;

    // if source is Master and we are NOT the TransportMaster, then
    // it falls back to SyncSourceTransport, change it in the view so the UI
    // doesn't have to deal with it
    // if there is no transport master, this track has the POTENTIAL to be the master
    // so leave it as Master
    if (tstate->syncSource == SyncSourceMaster) {
        int tmaster = state->syncState.transportMaster;
        if (tmaster > 0 && tmaster != tstate->number)
          tview->syncSource = SyncSourceTransport;
    }

    SyncState* ss = &(state->syncState);
    switch (tstate->syncSource) {
        case SyncSourceMidi:
            // suppress if no clocks
            if (!ss->midiReceiving)
              tview->syncTempo = ss->midiTempo;
            break;
        case SyncSourceHost:
            // suppress if transport is stopped?
            if (!ss->hostStarted)
              tview->syncTempo = ss->hostTempo;
            break;
        case SyncSourceTransport:
            // todo: don't really need to display this, the TransportElement
            // will almost always be visible
            tview->syncTempo = ss->transportTempo;
            break;
        default:
            break;
    }
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
void MobiusViewer::refreshTrackGroups(TrackState* tstate,  MobiusViewTrack* tview)
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

        MobiusConfig* config = provider->getOldMobiusConfig();
        
        // ignore if out of range
        if (newNumber > 0 && newNumber <= config->groups.size()) {
            GroupDefinition* group = config->groups[newNumber - 1];
            tview->groupName = group->name;
            tview->groupColor = group->color;
        }

        tview->refreshGroup = true;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
