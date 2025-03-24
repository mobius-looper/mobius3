/**
 * Translation between the SystemState and MobiusView
 * plus additions and simpliciations.
 *
 * Most of the information is contained in the MobiusViewTrack which drives the display of both
 * the main Status Area in the center, and the list of Track Strips at the bottom.  The Status Area
 * is able to show everything about the track, the Track Strips will have a subset.  This distinction
 * is a minor optimization that should not be assumed to last forever.
 *
 * The refrsh process is assembling the view from several sources.  Most of it comes from SystemState
 * which was the original way to communicate state from the engine to the UI.  Some is pulled
 * from configuration objects like Session and GroupDefinition based on references in SystemState.
 * And some is made by querying the engine for a few real-time parameter values.
 *
 * Beyond the capture of engine state, the view also contains support for analyzing complex changes that
 * trigger a refresh of portions of the UI.  For many UI components it is enough to simply remember the last
 * displayed value and compare it with the current value and trigger a repaint if they differ.
 *
 * Some like MinorModesElement require an analysis of many things to produce the displayed result.
 * Where different detection is more than just comparison of old/new values, the logic is being moved here,
 * making it easier to modify the UI without losing the difference detection code.  The UI can test a few
 * "refreshFoo" flags to see if something needs to be refreshed.  These should be considered extensions of
 * the view model that are only there for convenience, not a fundamental part of the model.
 *
 * The process of periodic UI refresh must proceed like this:
 *
 *      - maintenance thread reaches a refresh interval
 *      - current SystemState is obtained from the engine
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
 * Once created a MobiusViewTrack will remain in memory for the duration of the application.
 * If track counts are lowered, they are left behind for possible reuse.  The track view array
 * will grow as necessary to match the engine state.
 *
 */

#include <JuceHeader.h>

#include "../Supervisor.h"

#include "../model/SystemState.h"
#include "../model/TrackState.h"
#include "../model/Session.h"
#include "../model/SystemConfig.h"
#include "../model/GroupDefinition.h"

#include "MobiusView.h"
#include "MobiusViewer.h"

//////////////////////////////////////////////////////////////////////
//
// Initialization
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
 * The important thing here is that the view be fleshed out with the
 * same number of tracks as the initial session because the view can be used
 * during the startup process and it gets crashy if there isn't anything in it.
 *
 * I don't think it is necessary for the view tracks to actually resemble the
 * session tracks, just that they exist.
 */
void MobiusViewer::initialize(Session* session, MobiusView* view)
{
    grow(view, session->getTrackCount());

    // during the period before the first state refresh, it was important
    // to be able to use the view to verify track types, somehad an init
    // script or something that tried loading MIDI tracks or something
    // immediately after the plugin was installed and this failed
    // verification, unclear if that's still an issue but make
    // the view resemble what it will be after the first refresh
    for (int i = 0 ; i < session->getTrackCount() ; i++) {
        Session::Track* st = session->getTrackByIndex(i);
        MobiusViewTrack* mvt = view->tracks[i];
        mvt->type = st->type;
    }
    
    // always start on the first one on startup
    view->focusedTrack = 0;
    view->track = view->tracks[0];
}

/**
 * This does two things: It ensures that we have enough tracks
 * in the view to satisfy the SystemState that wants to deposit things
 * into them, and it sets totalTracks to be the number of tracks
 * that will actually be in use.
 */
void MobiusViewer::grow(MobiusView* view, int required)
{
    if (required == 0) {
        // crashy if we don't have at least one, force it
        Trace(1, "MobiusViewer: Forcing a single audio track, why?");
        required = 1;
    }

    for (int i = view->tracks.size() ; i < required ; i++) {
        MobiusViewTrack* vt = new MobiusViewTrack();
        vt->index = i;
        view->tracks.add(vt);
    }

    view->totalTracks = required;
}

/**
 * Reconfigure the view after changing the track counts in the session.
 *
 * Growing the track array isn't really necessary since this needs
 * to adapt to whatever comes back in the SystemState on each refresh.
 * There might be some things that will get confused if the count doesn't
 * immediately reflect the new session though.
 *
 * One important thing is that it needs to move the focused track
 * if the track count was reduced.
 */
void MobiusViewer::configure(Session* session, MobiusView* view)
{
    grow(view, session->getTrackCount());
    
    if (view->focusedTrack >= view->totalTracks) {
        // go to the highest or the first?
        view->focusedTrack = view->totalTracks - 1;
        view->track = view->tracks[view->focusedTrack];
    }
    
    // this now because the only version of SystemState we accept
    // during refresh, if you get one that doesn't match this it means
    // there was a stale state in the queue and should be ignored
    // until Kernel finishes building the new one

    // actually don't need this here, can compare during refresh
    //sessionVersion = session->getVersion();
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
    Session* session = provider->getSession();
    if (session->getVersion() != sysstate->sessionVersion) {
        Trace(2, "MobiusViewer: Ignoring refresh of stale state after session change");
        return;
    }

    // we normally pre-size the track arrays before sending it down but
    // if not, here we are
    if (view->tracks.size() < sysstate->totalTracks)
      Trace(1, "MobiusViewer: SystemState came back with more tracks than expected");

    // this doesn't just grow, it sets totalTracks in the view
    grow(view, sysstate->totalTracks);
    
    // sanity check on focus
    if (view->focusedTrack >= 0 && view->focusedTrack < view->totalTracks) 
      view->track = view->tracks[view->focusedTrack];
    else {
        Trace(1, "MobiusViewer: focusedTrack is out of whack");
        view->focusedTrack = 0;
        view->track = view->tracks[0];
    }
    
    // Counter needs this
    view->sampleRate = provider->getSampleRate();

    resetRefreshTriggers(view);

    // detect when the selected track changes so we do a full refresh
    if (view->lastFocusedTrack != view->focusedTrack) {
        view->trackChanged = true;
        view->lastFocusedTrack = view->focusedTrack;
    }

    refreshAllTracks(sysstate, view);

    // dump the entire sync state over, no need to duplicate
    view->syncState = sysstate->syncState;

    // so the display elements don't have to test for view->trackChanged
    // in addition to the element specific refresh flags, if at the end of refresh
    // trackChanged is set, force all the secondary flags on
    if (view->trackChanged)
      forceRefresh(view);

    // This is used to track the progress of an edited or different Session that
    // was sent to the engine.  When this number comes back different it means that
    // significant changes were made to the session and it should  trigger a full refresh.
    if (view->lastSessionVersion != sysstate->sessionVersion) {
        forceRefresh(view);
        view->lastSessionVersion = sysstate->sessionVersion;
    }
    
}

/**
 * Called at the beginning of every refresh cycle to reset the refresh
 * trigger flags.  Some components may clear these as a side effect of paint
 * but this should not be required.
 */
void MobiusViewer::resetRefreshTriggers(MobiusView* view)
{
    view->trackChanged = false;
    
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
// Tracks
//
//////////////////////////////////////////////////////////////////////

/**
 * This is what we should be doing for all tracks as soon as core
 * refreshes the new TrackState model properly.
 */
void MobiusViewer::refreshAllTracks(SystemState* state, MobiusView* view)
{
    for (int i = 0 ; i < state->totalTracks ; i++) {

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

    if (view->focusedTrack < 0 || view->focusedTrack >= view->totalTracks) {
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
 * Refresh a track view from the new TrackState model.
 */ 
void MobiusViewer::refreshTrack(SystemState* state, TrackState* tstate,
                                MobiusView* mview, MobiusViewTrack* tview)
{
    tview->type = tstate->type;
    tview->active = tstate->active;

    // noticed that since MidiTrack doesn't set the active flag it can stay
    // stuck on if you reorder tracks and reuse the same TrackState for a differnet
    // track type.  this is actually a bigger problem.  Each track type either needs
    // to fully reset the TrackState, or we need to do it before sending it down
    if (tview->type == Session::TypeMidi)
      tview->active = false;
    
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

    juce::String newMode = deriveMode(tstate);
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
 * Derive the display mode.
 * This starts with the actual operating mode from the loop but
 * can be modified for display when combined with other state flags.
 *
 * We can only show one primary mode so the precendence of these is
 * debatable but this seems to make the most sense.
 */
juce::String MobiusViewer::deriveMode(TrackState* tstate)
{
    juce::String mode;
    
    if (tstate->pause)
      mode = "Pause";

    else if (tstate->switchConfirm)
      mode = "Confirm";

    else if (tstate->nextLoop > 0)
      mode = "Switch";

    else if (tstate->returnLoop > 0)
      mode = "Return";

    else if (tstate->globalMute)
      mode = "Global Mute";

    else if (tstate->mode == TrackState::ModePlay && tstate->overdub)
      mode = "Overdub";
    
    // todo: OldMobiusState also supported GlobalPause
    // GlobalPause seems to have been broken for some time
    // Track::mGlobalMute is set by the Mute function
    // Solo is wound up in this too

    else
      mode = TrackState::getModeName(tstate->mode);

    return mode;
}

/**
 * This formerly monitored a "setupChanged" flag to trigger the refresh.
 * Unclear why this optimization was felt necessary.
 */
void MobiusViewer::refreshTrackName(SystemState* state, TrackState* tstate,
                                    MobiusView* mview, MobiusViewTrack* tview)
{
    (void)mview;
    (void)state;
    tview->name = "";
    Session* session = provider->getSession();
    Session::Track* track = session->getTrackByIndex(tstate->number - 1);
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

    // new variant of the speed shit for MIDI
    if (tstate->rate != tview->rate) {
        tview->rate = tstate->rate;
        // is this necessary?
        //tview->anySpeed = true;
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

    if (tview->rate != 0.0f && tview->rate != 1.0f)
      tview->minorModes.add(juce::String("Rate ") + juce::String(tview->rate));
    
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
    tview->trackSyncUnit = tstate->trackSyncUnit;
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

    // these are latching and cleared by the Beaters element when it displays
    // todo: we've got three levels of latching around this
    // Loop transferring to TrackState, here transferring to MobiusViewTrack
    // and another in BeatersElement, don't need all of these, do we?
    // update: This is now handled by PriorityState for the focused track only
    // Loop.cpp no longer sets these flags in the TrackState and BeatersElement
    // only looks at PriorityState
    if (!tview->beatLoop)
      tview->beatLoop = tstate->beatLoop;
    tstate->beatLoop = false;
    
    if (!tview->beatCycle)
      tview->beatCycle = tstate->beatCycle;
    tstate->beatCycle = false;
    
    if (!tview->beatSubcycle)
      tview->beatSubcycle = tstate->beatSubcycle;
    tstate->beatSubcycle = false;
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

        if (newNumber > 0) {
            SystemConfig* config = provider->getSystemConfig();
            GroupDefinitions* groups = config->getGroups();
            if (groups != nullptr) {
                GroupDefinition* group = groups->getGroupByIndex(newNumber - 1);
                if (group != nullptr) {
                    tview->groupName = group->name;
                    tview->groupColor = group->color;
                }
            }
        }
        
        tview->refreshGroup = true;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
