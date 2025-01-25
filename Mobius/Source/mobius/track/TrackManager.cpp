
#include <JuceHeader.h>

#include "../../util/StructureDumper.h"
#include "../../model/Symbol.h"
#include "../../model/FunctionProperties.h"
#include "../../model/ScriptProperties.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Session.h"
#include "../../model/UIAction.h"
#include "../../model/UIParameter.h"
#include "../../model/Query.h"
#include "../../model/Scope.h"
#include "../../model/SystemState.h"

#include "../../script/MslExternal.h"
#include "../../script/MslWait.h"
#include "../../script/ScriptExternals.h"

#include "../MobiusKernel.h"
#include "../MobiusInterface.h"

#include "LogicalTrack.h"

#include "../midi/MidiWatcher.h"
#include "../midi/MidiTrack.h"

#include "../core/Mobius.h"

#include "TrackManager.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

TrackManager::TrackManager(MobiusKernel* k) : mslHandler(k, this)
{
    kernel = k;
    actionPool = k->getActionPool();
    watcher.initialize(&(pools.midiPool));
}

TrackManager::~TrackManager()
{
    tracks.clear();
}

/**
 * Startup initialization.  Session here is normally the default
 * session, a different one may come down later via loadSession()
 */
void TrackManager::initialize(MobiusConfig* config, Session* ses, Mobius* core)
{
    // only thing this is used for is the group list
    configuration = config;
    session = ses;
    audioEngine = core;

    // this isn't owned by MidiPools, but it's convenient to bundle
    // it up with the others
    pools.actionPool = kernel->getActionPool();
    scopes.refresh(config);

    // start with this here, but should move to Kernel once
    // Mobius can use it too
    longWatcher.initialize(session, kernel->getContainer()->getSampleRate());
    longWatcher.setListener(this);

    loadSession(ses);
}

/**
 * Have to get this to refresh GroupDefinitions
 */
void TrackManager::configure(MobiusConfig* config)
{
    configuration = config;
    scopes.refresh(config);
}

/**
 * Reconfigure the LogicalTracks based on the session.
 *
 * The Session is expected to have been normalized with audioCount and midiCount
 * accurate and a Session::Track for each one.
 *
 * Tracks will have been numbered starting from 1 with the audio tracks first
 * but the order of the Session::Track array is random.
 *
 * Changing the audio track count is awkward because it is still done through the Setup
 * which must have already been processed by the core.
 *
 */
void TrackManager::loadSession(Session* s) 
{
    session = s;
    longWatcher.initialize(session, kernel->getContainer()->getSampleRate());

    // allow this to be disabled during debugging
    longDisable = session->getBool("longDisable");

    configureTracks(session);

    // !! the relationship here is old and stupid
    // Tracks don't actually listen to each other, the only TrackListener
    // is TrackManager itself and it registers for every track, and then
    // Notifer goes through the Logicaltrack for the notification, which
    // then forwards back to TrackManager
    // unless there need to be other TrackListeners besides us,
    // tracks can always just inform TrackManager whenever it does
    // something and we can skip going through Notifier
    
    for (int i = 0 ; i < tracks.size() ; i++) {
        LogicalTrack* lt = tracks[i];
        lt->addTrackListener(this);
    }
}

/**
 * Organize the track array for a new session.
 *
 * The Session gets to be authoritative over the track order and numbering.
 * At the moment it will always put the audio tracks first, and the engine
 * will have been configured to have that many.  
 *
 * The Session must have normalized track numbers.  Session Tracks can be in any order.
 */
void TrackManager::configureTracks(Session* ses)
{
    (void)ses;  // already saved this in a member
    
    // transfer the current track list to a holding area
    juce::Array<LogicalTrack*> oldTracks;
    while (tracks.size() > 0) {
        LogicalTrack* lt = tracks.removeAndReturn(0);
        oldTracks.add(lt);
    }

    // now put them back or create new ones
    for (int i = 0 ; i < session->getTrackCount() ; i++) {
        Session::Track* def = session->getTrackByIndex(i);

        // see if we had one with this id before
        LogicalTrack* lt = nullptr;
        int index = 0;
        for (auto old : oldTracks) {
            if (old->getSessionId() == def->id) {
                // reuse this one
                lt = oldTracks.removeAndReturn(index);
                break;
            }
            index++;
        }

        if (lt == nullptr)
          lt = new LogicalTrack(this);
        tracks.add(lt);

        // this remembers it but does not act on it
        lt->setSession(def);
    }
    
    configureMobiusTracks();
        
    // tell the tracks to process the session AFTER the track array has been reorganized
    // so they can do things that may check relationships with other tracks
    // this is where MidiTracks or other implementations happen
    for (auto track : tracks) {
        track->loadSession();
    }
    
    while (oldTracks.size() > 0) {
        LogicalTrack* lt = oldTracks.removeAndReturn(0);
        Trace(2, "TrackManager: Removing unused track");
        delete lt;
    }
}

/**
 * After the main LogicalTrack list is fleshed out, extract just the MobiusLooperTracks
 * to be sent over to Mobius for track configuration.
 */
void TrackManager::configureMobiusTracks()
{
    juce::Array<MobiusLooperTrack*> mtracks;

    for (auto ltrack : tracks) {
        MobiusLooperTrack* mlt = ltrack->getMobiusTrack();
        if (mlt != nullptr)
          mtracks.add(mlt);
    }

    // pass these over
    audioEngine->configureTracks(mtracks);
}

//////////////////////////////////////////////////////////////////////
//
// Information and Services
//
//////////////////////////////////////////////////////////////////////

MidiPools* TrackManager::getPools()
{
    return &pools;
}

MobiusConfig* TrackManager::getConfiguration()
{
    return configuration;
}

Session* TrackManager::getSession()
{
    return session;
}

MobiusContainer* TrackManager::getContainer()
{
    return kernel->getContainer();
}

SyncMaster* TrackManager::getSyncMaster()
{
    return kernel->getSyncMaster();
}

SymbolTable* TrackManager::getSymbols()
{
    return kernel->getContainer()->getSymbols();
}

MslEnvironment* TrackManager::getMsl()
{
    return kernel->getContainer()->getMslEnvironment();
}

Mobius* TrackManager::getAudioEngine()
{
    return audioEngine;
}

// this needs to be implemented here rather than going back to the container
// also, start passing this around as a number rather than an index like everything else
int TrackManager::getFocusedTrackIndex()
{
    return kernel->getContainer()->getFocusedTrackIndex();
}

int TrackManager::getMidiOutputDeviceId(const char* name)
{
    return kernel->getMidiOutputDeviceId(name);
}

MidiEvent* TrackManager::getHeldNotes()
{
    return watcher.getHeldNotes();
}

LogicalTrack* TrackManager::getLogicalTrack(int number)
{
    LogicalTrack* track = nullptr;
    int index = number - 1;
    if (index >= 0 && index < tracks.size())
      track = tracks[index];
    else
      Trace(1, "TrackManager: Invalid lgical track number %d", number);
    return track;
}

void TrackManager::getTrackProperties(int number, TrackProperties& props)
{
    LogicalTrack* lt = getLogicalTrack(number);
    if (lt != nullptr)
      lt->getTrackProperties(props);
    else
      props.invalid = true;
    
    // Mobius doesn't set this, caller should get it consistently
    props.number = number;
}

//////////////////////////////////////////////////////////////////////
//
// Inbound Events
//
//////////////////////////////////////////////////////////////////////

/**
 * To start out, we'll be the common listener for all tracks but eventually
 * it would be better for MidiTracks to do it themselves based on their
 * follower settings.  Would save some unnessary hunting here.
 */
void TrackManager::trackNotification(NotificationId notification, TrackProperties& props)
{
    for (auto track : tracks) {
        track->trackNotification(notification, props);
    }
}

/**
 * An event comes in from one of the MIDI devices, or the host.
 * For notes, a shared hold state is maintained in Watcher and can be used
 * by each track to include notes in a record region that went down before they
 * were recording, and are still held when they start recording.
 *
 * The event is passed to all tracks, if a track wants to record the event
 * it must make a copy.
 *
 * !! The event is tagged with the MidiManager device id, but if this
 * is a plugin we reserve id zero for the host, so they need to be bumped
 * by one if that becomes significant
 *
 * Actually hate using MidiEvent for this because MidiManager needs to have
 * a pool, but we won't share it so it's always allocating one.  Just
 * pass the MidiMessage down
 */
void TrackManager::midiEvent(MidiEvent* e)
{
    // watch it first since tracks may reach a state that needs it
    watcher.midiEvent(e);

    for (auto track : tracks)
      track->midiEvent(e);
    
    pools.checkin(e);
}

/**
 * An event comming in from the plugin host, via Kernel
 */
void TrackManager::midiEvent(juce::MidiMessage& msg, int deviceId)
{
    MidiEvent* e = pools.newEvent();
    e->juceMessage = msg;
    e->device = deviceId;
    midiEvent(e);
}

//////////////////////////////////////////////////////////////////////
//
// Outbound Events
//
//////////////////////////////////////////////////////////////////////

void TrackManager::alert(const char* msg)
{
    kernel->sendMobiusMessage(msg);
}

void TrackManager::midiSend(juce::MidiMessage& msg, int deviceId)
{
    kernel->midiSend(msg, deviceId);
}

void TrackManager::writeDump(juce::String file, juce::String content)
{
    kernel->getContainer()->writeDump(file, content);
}

/**
 * Only need to support following of audio tracks right now so can go
 * directly to Mobius.
 */
int TrackManager::scheduleFollowerEvent(int audioTrack, QuantizeMode q, int followerTrack, int eventId)
{
    return audioEngine->scheduleFollowerEvent(audioTrack, q, followerTrack, eventId);
}

//////////////////////////////////////////////////////////////////////
//
// Audio Block Lifecycle
//
//////////////////////////////////////////////////////////////////////

/**
 * This must be called early during audio block processing to prepare
 * for incomming actions and the stream.
 *
 * Formerly maintained a stupid queued action list for the core, now there
 * is nothing extra to do, but leave in place in case MIDI tracks need
 * something someday.
 */
void TrackManager::beginAudioBlock()
{
}

/**
 * Advance the longWatcher which may cause more actions to fire.
 */
void TrackManager::advanceLongWatcher(int frames)
{
    // advance the long press detector, this may call back
    // to longPressDetected to fire an action
    // todo: Mobius has one of these too, try to merge
    if (!longDisable)
      longWatcher.advance(frames);
}

#if 0
/**
 * The root of the audio block processing for all tracks.
 */
void TrackManager::processAudioStream(MobiusAudioStream* stream)
{
    // todo: need to be using SyncMaster to order the advance
    // of both the audio tracks and the MIDI tracks
    
    // advance audio core
    audioEngine->processAudioStream(stream);
    
    // then advance the MIDI tracks
    for (auto track : tracks)
      track->processAudioStream(stream);
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the interface most things call.
 * Only MSL needs results.
 *
 * Ownership of the action transfers and it will be pooled.
 */
void TrackManager::doAction(UIAction* src)
{
    ActionResult result;
    doActionWithResult(src, result);
}

/**
 * Distribute an action to the LogicalTracks.
 * This only handles LevelCore actions, Kernel will have already
 * dealt with upward actions.  Kernel also handled script actions.
 */
void TrackManager::doActionWithResult(UIAction* src, ActionResult& result)
{
    Symbol* s = src->symbol;
    SymbolId sid = s->id;
    
    // watch long before replication
    // could also watch after but this would generate many long actions
    // which could then all be duplicated
    if (!longDisable)
      longWatcher.watch(src);

    if (sid == FuncDump) {
        // at what level should this be handled?
        StructureDumper d;
        int focusedNumber = getFocusedTrackIndex() + 1;
        LogicalTrack* lt = getLogicalTrack(focusedNumber);
        lt->dump(d);
        writeDump(juce::String("LogicalTrack.txt"), d.getText());
        actionPool->checkin(src);
    }
    else if (sid == FuncNextTrack || sid == FuncPrevTrack || sid == FuncSelectTrack) {
        // special case for track selection functions
        doTrackSelectAction(src);
    }
    else if (s->functionProperties != nullptr && s->functionProperties->global) {
        // globals are weird
        doGlobal(src);
    }
    else if (s->behavior == BehaviorActivation) {
        doActivation(src);
    }
    else if (s->behavior == BehaviorSample) {
        // Kernel should have caught this
        Trace(1, "TrackManager: BehaviorSample action leaked down");
    }
    else if (s->behavior == BehaviorScript || s->script != nullptr) {
        doScript(src);
    }
    else {    
        // function or parameter
        // replicate the source action to one or more actions with specific track scopes
        UIAction* actions = replicateAction(src);
        sendActions(actions, result);
    }
}

/**
 * Send a list of actions to one of the two sides, and return the actions to the pool.
 * Here we do filtering of functions that can only be used with certain tracks.
 * Could have done that during replication as well, but it's easier to let that mess
 * finish and suppress it here.  This could also be a place where we do last minute
 * adjustments so the action function is actually changed to something suitable for that
 * track type.
 *
 * Could do the same for parameters.   It doesn't hurt to send it through, but it generates
 * log errors if it doesn't make sense.
 *
 * ActionResult can only handle one result, which is all we need for scripts
 * atm aince it handles focus and group bindings internally and will always
 * send down a track-scoped action.
 */
void TrackManager::sendActions(UIAction* actions, ActionResult& result)
{
    while (actions != nullptr) {
        UIAction* next = actions->next;
        // internal components want to use next for their own use so
        // make sure it starts empty
        actions->next = nullptr;
            
        int number = actions->getScopeTrack();
        if (number == 0) {
            // should not see this after replication
            Trace(1, "TrackManager: Action replication is busted");
        }
        else {
            LogicalTrack* lt = getLogicalTrack(number);
            if (lt != nullptr) {
                bool sendIt = true;
                Symbol* s = actions->symbol;
                if (s->functionProperties != nullptr &&
                    s->functionProperties->midiOnly) {
                    sendIt = (lt->getType() == Session::TypeMidi);
                }
                if (sendIt)
                  lt->doAction(actions);
            }
        }

        // remember the last result
        result.coreEvent = actions->coreEvent;
        result.coreEventFrame = actions->coreEventFrame;

        actionPool->checkin(actions);
        actions = next;
    }
}

/**
 * There are only two types of activations: Setups and Presets.
 * We don't have a properties model for activations so we have to derive
 * it from the name prefix, but Setup activations are "global" in that they
 * do not apply to any particular track, and they only apply to audio tracks.
 *
 * Presets can be track specific and need replication, but they only apply
 * audio tracks.  
 */
void TrackManager::doActivation(UIAction* src)
{
    juce::String name = src->symbol->name;
    if (name.startsWith(Symbol::ActivationPrefixSetup)) {
        // it doesn't matter what track this goes to
        audioEngine->doAction(src);
        actionPool->checkin(src);
    }
    else if (name.startsWith(Symbol::ActivationPrefixPreset)) {
        // technically we should keep these out of MIDI tracks if they're
        // included due to focus or group membership, it's okay to leave
        // them in as long as they ignore the activation request without
        // whining about it in the log, ideally should pass "doMidi and doAudio"
        // flags to replicateAction
        UIAction* actions = replicateAction(src);
        // don't need results on these
        ActionResult results;
        sendActions(actions, results);
    }
    else {
        // you snuck in another activation type without telling me, bastard
        Trace(1, "TrackManager: Unknown activation type %s", name.toUTF8());
        actionPool->checkin(src);
    }
}

/**
 * Here for a script symbol.
 * If this is a MOS script it only goes to the audio side, MSL can
 * go to both sides.
 *
 * Focus lock for MOS scripts is more complex than usual, I think the script
 * has to say whether it supports focus and this is handled deep within
 * Actionator, the RunScriptFunction and some combination of the old
 * script runtime classes.  I don't want to break that right now, so just
 * pass it through the old way.  This may cause some of my new warnings
 * to fire if it lands in the Actionator code that deals with unspecified
 * action scopes.  Eventually move all this out here if possible.
 * Start by making MSL scripts have "functionness" and see what shakes out of that,
 * then can handle MOS the same way.
 *
 * good news: !focusLock in old scripts worked if you just let it pass through
 * and it didn't hit our warnings.  I think this is because RunScriptFunction
 * is flagged as global and internally handled as global.  And must do focus lock
 * in a different and certainly shitty way.
 */
void TrackManager::doScript(UIAction* src)
{
    Symbol* sym = src->symbol;
    
    if (sym->script == nullptr) {
        // can't have ScriptHavior without ScriptProperties
        Trace(1, "TrackManager: Script behavior action without properties %s",
              sym->getName());
    }
    else if (sym->script->coreScript) {
        // it's an old one, only send this to core tracks
        // we won't be replicating these for focus or groups, if the scope track is out
        // of range ignore, if the scope is not specified, send it to the first core track
        // and let Mobius sort out where it goes
        int number = src->getScopeTrack();
        if (number == 0) {
            audioEngine->doAction(src);
        }
        else {
            LogicalTrack* lt = getLogicalTrack(number);
            if (lt != nullptr) {
                if (lt->getType() != Session::TypeAudio)
                  Trace(1, "TrackManager: MOS scripts can't be sent to MIDI tracks");
                else
                  audioEngine->doAction(src);
            }
        }
    }
    else if (sym->script->mslLinkage != nullptr) {
        // it's a new one
        // kernel is intercepting these and doing this
        
        //ActionAdapter aa;
        //aa.doAction(container->getMslEnvironment(), this, action);

        // let's continue with letting MSL scripts run at a higher level than
        // function action handling until we need to, it's really more about the
        // functions the script CAUSES than the running of the script itself,
        // though it could be desireable to have a script that behaves just like
        // a function for quantization, event stacking, undo, etc.  that would be more
        // like how MOS scripts behave
        Trace(1, "TrackManager: MSL Script action received and we weren't expecting that, no sir");
    }
    else {
        Trace(1, "TrackManager: Malformed ScriptProperties on %s", sym->getName());
    }
    
    actionPool->checkin(src);
}

/**
 * Replicate the action if necessary for groups and focus lock.
 * The original action should be considered "consumed" and the returned
 * list are all actions from the pool.  Each action will be given a
 * track-specific scope.  From here on down, groups and focus lock do not
 * need to be considered and we can start ripping that out of old Mobius code.
 */
UIAction* TrackManager::replicateAction(UIAction* src)
{
    UIAction* list = nullptr;

    if (src->noGroup) {
        // noGroup is an obscure flag set in Scripts to disable group/focus lock handling
        // for this action,  old Mobius always sent this to the active/focused track
        // I think it makes sense to obey track scope if one was set before falling
        // back to focused track
        int track = scopes.parseTrackNumber(src->getScope());
        if (track > 0) {
            // they asked for this track, that's what they'll get
            list = src;
        }
        else {
            // send it to the focused track
            src->setScopeTrack(getFocusedTrackIndex() + 1);
            list = src;
        }
    }
    else if (src->hasScope()) {
        int track = scopes.parseTrackNumber(src->getScope());
        if (track > 0) {
            // targeting a specific track
            // focus lock does not apply here but group focvus replication might
            // unclear what this should do, the most recent implementation
            // of "Group Have Focus Lock" did not do replication if there
            // was an explicit track scope in the action, not sure what old
            // Mobius did, but this is an obscure option and the one guy that
            // uses it seems happy with this
            list = src;
        }
        else {
            int ordinal = scopes.parseGroupOrdinal(src->getScope());
            if (ordinal >= 0) {
                // repliicate to all members of this group
                // on the track group association is by number rather than ordinal
                list = replicateGroup(src, ordinal + 1);
            }
            else {
                Trace(1, "TrackManager: Invalid scope %s", src->getScope());
            }
        }
    }
    else {
        // no scope, send it to the focused track,
        // and other members of the focused track's group if the special
        // group option is on
        list = replicateFocused(src);
    }
    return list;
}

/**
 * Replicate this action to all members of a group
 * Group is specified by ordinal which is what old Mobius Track uses
 */
UIAction* TrackManager::replicateGroup(UIAction* src, int group)
{
    UIAction* list = nullptr;

    for (auto track : tracks) {
        int tgroup = track->getGroup();
        if (tgroup == group)
          list = addAction(list, src, track->getNumber());
    }
    
    // didn't end up using this to reclaim it
    actionPool->checkin(src);
    // final list may be empty if there were no tracks in this group
    return list;
}

/**
 * Helper to maintain the list of replicated actions
 */
UIAction* TrackManager::addAction(UIAction* list, UIAction* src, int targetTrack)
{
    UIAction* copy = actionPool->newAction();
    copy->copy(src);
    copy->setScopeTrack(targetTrack);
    copy->next = list;
    return copy;
}

/**
 * Replicate this action to a the focused track and all other tracks
 * that have focus lock.
 *
 * If the focused track is in a group and that group has the "Group Focus Lock"
 * option enabled, then also replicate to other members of that group.
 */
UIAction* TrackManager::replicateFocused(UIAction* src)
{
    UIAction* list = nullptr;

    // find the group number of the focused track
    int focusedGroupNumber = 0;
    GroupDefinition* groupdef = nullptr;
    int focusedNumber = getFocusedTrackIndex() + 1;
    LogicalTrack* lt = getLogicalTrack(focusedNumber);
    if (lt != nullptr) {
        focusedGroupNumber = lt->getGroup();
        // get the definition from the number
        if (focusedGroupNumber > 0) {
            int groupIndex = focusedGroupNumber - 1;
            if (groupIndex < configuration->groups.size())
              groupdef = configuration->groups[groupIndex];
        }
    }

    // now add focused tracks
    for (auto track : tracks) {
        int tgroup = track->getGroup();
        if ((focusedNumber == track->getNumber()) ||
            track->isFocused() ||
            (tgroup == focusedGroupNumber && isGroupFocused(groupdef, src))) {
            list = addAction(list, src, track->getNumber());
        }
    }
    
    // didn't end up using this to reclaim it
    actionPool->checkin(src);
    // final list will always have at least the focused track
    return list;
}

/**
 * When a target track is in a group we've got the old confusing
 * "groups have focus lock" option which is now called "Enable Group Repliation".
 */
bool TrackManager::isGroupFocused(GroupDefinition* def, UIAction* src)
{
    bool focused = false;

    if (def != nullptr && def->replicationEnabled) {
        Symbol* s = src->symbol;
        // we have a group and it allows replication
        // only do this for certain functions and parameters
        if (src->symbol->functionProperties != nullptr) {
            focused = (def->replicatedFunctions.contains(s->name));
        }
        else if (src->symbol->parameterProperties != nullptr) {
            focused = (def->replicatedParameters.contains(s->name));
        }
    }
    return focused;
}

/**
 * Perform a global function
 * These don't have focus or replication.
 * It's weird because the old Mobius core had it's own complex handling
 * for global functions and I don't want to disrupt that.  So just send
 * the action down to the first track, it doesn't matter what the action scope is.
 *
 * MIDI tracks do not have any special handling for global functions, they are
 * simply duplicated for each track.
 */
void TrackManager::doGlobal(UIAction* src)
{
    // first send it to all midi tracks, they won't trash the action
    for (auto track : tracks) {
        if (track->getType() != Session::TypeAudio)
          track->doAction(src);
    }

    // then send it to the first audio track
    src->setScopeTrack(1);
    audioEngine->doAction(src);

    // having some trouble with stuck notes in the watcher
    // maybe only during debugging, but it's annoying when it happens to
    // make sure to clear them
    if (src->symbol->id == FuncGlobalReset)
      watcher.flushHeld();

    actionPool->checkin(src);
}

/**
 * Special case for the track selection functions.  These are weird, they're
 * kind of a global function, and kind of a UI level function, but they can be
 * used in scripts and I don't want to throw it all the way back up async if
 * the action starts down in the kernel.
 *
 * In the logical track space, there can be a mixture of audio and MIDI tracks
 * so the old implementation in core doesn't work.  After deciding which track
 * should have focus IF it is an audio track we send down a SelectTrack action
 * with the core track number rather than letting core next/prev among just
 * the audio tracks.  This also selects what core considered the "active" track.
 *
 * If the new track is not audio, then nothing is sent to the core and the active
 * track stays what it was.  This is confusing only for MOS scripts that won't
 * see the full logical track space.  
 *
 * !! The focused track really needs to be something maintained authoritatively
 * by TrackManager and passed up in the State, rather than letting Supervisor
 * maintain it in the view and requiring us to notify it when it changes out
 * from under the view.
 *
 * One significant side effect of changing tracks is the EmptyTrackAction.  This
 * will still work for core tracks, but the "source" track for content copy can
 * only be whatever is the current active track.  We do not have EmptyTrackAction
 * for Midi tracks yet as there is no concept of the "active" MidiTrack.  Should
 * do that someday for consistency but it starts to be confusing in the UI as to what
 * will happen if they're not all adjacent.
 */
void TrackManager::doTrackSelectAction(UIAction* a)
{
    Symbol* s = a->symbol;
    int prevFocusedIndex = getFocusedTrackIndex();
    int newFocusedIndex = prevFocusedIndex;
    int maxIndex = session->getTrackCount() - 1;

    // note tht we're dealing with Numbers not Indexes
    if (s->id == FuncNextTrack) {
        newFocusedIndex = prevFocusedIndex + 1;
        if (newFocusedIndex > maxIndex)
          newFocusedIndex = 0;
    }
    else if (s->id == FuncPrevTrack) {
        newFocusedIndex = prevFocusedIndex - 1;
        if (newFocusedIndex < 0)
          newFocusedIndex = maxIndex;
    }
    else if (s->id == FuncSelectTrack) {
        // action argument is 1 based, convert to index
        newFocusedIndex = a->value - 1;
        if (newFocusedIndex < 0 || newFocusedIndex > maxIndex) {
            Trace(1, "TrackManager: Bad SelectTrack argument");
            newFocusedIndex = prevFocusedIndex;
        }
    }
    else {
        Trace(1, "TrackManager::doTrackSelectAction Not a track selection function");
    }

    if (newFocusedIndex != prevFocusedIndex) {

        // now look at what the new track is, back to numbers 
        LogicalTrack* lt = getLogicalTrack(newFocusedIndex + 1);
        if (lt == nullptr) {
            Trace(1, "TrackManager: Select track out of wack");
        }
        else if (lt->getType() == Session::TypeAudio) {

            // adjust the action to look like SelectTrack with an argument
            a->symbol = kernel->getContainer()->getSymbols()->getSymbol(FuncSelectTrack);

            // NOTE: this is an example of an action VALUE needing to be transformed into
            // a core track number, usually it is just the scope that is changed
            // by MobiusLooperTrack.  This is NOT general, we happen to know that SelectTrack
            // uses track numbers as arguments but there may be others
            // perhaps it would be better to have SelectTrack use the scope without an argument
            // and have that treated as self selection.
            // MobiusLooperTrack can't really handle this because it doesn't know enough
            // about the logical track space to make the transformation
            a->value = lt->getEngineNumber();

            // for the scope, it doesn't matter what it is since MobiusLooperTrack will
            // always force actions and queries to have the scope of the track it is
            // connected to
            a->setScopeTrack(0);

            lt->doAction(a);
        }
        else {
            // MIDI tracks would only care if they supported EmptyTrackAction
            // and the notion of an active track
        }

        // until we have returning focus changes in the State, have to inform
        // the UI that it changed
        kernel->getContainer()->setFocusedTrack(newFocusedIndex);
    }
    
    actionPool->checkin(a);
}

//////////////////////////////////////////////////////////////////////
//
// Long Press
//
//////////////////////////////////////////////////////////////////////

/**
 * Listener callback for LongWatcher.
 * We're inside processAudioStream and one of the watchers
 * has crossed the threshold
 */
void TrackManager::longPressDetected(LongWatcher::State* state)
{
    //Trace(2, "TrackManager::longPressDetected %s", a->symbol->getName());
    //doMidiTrackAction(src);

    // quick and dirty for the only one people use
    if (state->symbol->id == FuncRecord) {

        if (state->notifications < 2) {
            // everything else expects these to be pooled
            UIAction* la = actionPool->newAction();

            if (state->notifications == 0) {
                Trace(2, "TrackManager: Long Record to Reset");
                la->symbol = getSymbols()->getSymbol(FuncReset);
            }
            else {
                Trace(2, "TrackManager: LongLong Record to TrackReset");
                la->symbol = getSymbols()->getSymbol(FuncTrackReset);
            }
            // would be nice to have this extend to GlobalReset but
            // would have to throw that back to Kernel

            la->value = state->value;
            la->setScope(state->scope);
            CopyString(state->arguments, la->arguments, sizeof(la->arguments));

            // !! one difference doing it this way is with group focus replication
            // which is limited to certain functions  if Record is on the list but not
            // Reset, then the Reset will be ignored, whereas before it would be a Record
            // action with the long flag which would pass
            // could work around this by adding something to the action like
            // originalSymbol or triggerSymbol that is used to test for passage
            
            doAction(la);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Query
//
//////////////////////////////////////////////////////////////////////

/**
 * Just asking questions...
 */
bool TrackManager::doQuery(Query* q)
{
    bool status = false;
    
    if (q->symbol == nullptr) {
        Trace(1, "TrackManager: UIAction without symbol, you had one job");
    }
    else {
        int trackNumber = q->scope;
        if (trackNumber <= 0)
          trackNumber = getFocusedTrackIndex() + 1;

        LogicalTrack* lt = getLogicalTrack(trackNumber);
        if (lt != nullptr)
          status = lt->doQuery(q);
    }
    return status;
}

/**
 * MSL queries can be for symbol queries or internal variables.
 * TrackMslHandler should be doing all of this now?
 */
bool TrackManager::mslQuery(MslQuery* query)
{
    bool success = false;
    ScriptExternalType type = (ScriptExternalType)(query->external->type);

    if (type == ExtTypeSymbol) {
        Query q;
        q.symbol = static_cast<Symbol*>(query->external->object);
        q.scope = query->scope;

        (void)doQuery(&q);

        mutateMslReturn(q.symbol, q.value, &(query->value));

        // Query at this level will never be "async"
        success = true;
    }
    else {
        // here we have the problem of scope trashing since we need to
        // direct it to one side or the other and be specific, I don't
        // think MslSession cares, but be safe
        int saveScope = query->scope;
        if (query->scope == 0)
          query->scope = kernel->getContainer()->getFocusedTrackIndex() + 1;

        LogicalTrack* lt = getLogicalTrack(query->scope);
        if (lt != nullptr) {
            // MobiusLooperTrack now provides this
            //if (lt->getType() == Session::TypeAudio)
            //success = audioEngine->mslQuery(query);
            //else
            success = mslHandler.mslQuery(lt, query);
        }

        // in case we trashed it
        query->scope = saveScope;
    }
    return success;
}

bool TrackManager::mslQuery(VarQuery* query)
{
    bool success = false;
    // here we have the problem of scope trashing since we need to
    // direct it to one side or the other and be specific, I don't
    // think MslSession cares, but be safe
    int saveScope = query->scope;
    if (query->scope == 0)
      query->scope = kernel->getContainer()->getFocusedTrackIndex() + 1;

    LogicalTrack* lt = getLogicalTrack(query->scope);
    if (lt != nullptr) {
        // MobiusLooperTrack now provides this
        //if (lt->getType() == Session::TypeAudio)
        //success = audioEngine->mslQuery(query);
        //else
        success = mslHandler.varQuery(lt, query);
    }

    // in case we trashed it
    query->scope = saveScope;
    return success;
}

/**
 * Convert a query result that was the value of an enumerated parameter
 * into a pair of values to return to the interpreter.
 * Not liking this but it works.  Supervisor needs to do exactly the same
 * thing so it would be nice to share this.  The only difference
 * is the way we have to call ParameterHelper.
 */
void TrackManager::mutateMslReturn(Symbol* s, int value, MslValue* retval)
{
    if (s->parameter == nullptr) {
        // no extra definition, return whatever it was
        retval->setInt(value);
    }
    else {
        UIParameterType ptype = s->parameter->type;
        if (ptype == TypeEnum) {
            // don't use labels since I want scripters to get used to the names
            const char* ename = s->parameter->getEnumName(value);
            retval->setEnum(ename, value);
        }
        else if (ptype == TypeBool) {
            retval->setBool(value == 1);
        }
        else if (ptype == TypeStructure) {
            // hmm, the understanding of LevelUI symbols that live in
            // UIConfig and LevelCore symbols that live in MobiusConfig
            // is in Supervisor right now
            // todo: Need to repackage this
            // todo: this could also be Type::Enum in the value but I don't
            // think anything cares?
            retval->setJString(kernel->getContainer()->getStructureName(s, value));
        }
        else {
            // should only be here for TypeInt
            // unclear what String would do
            retval->setInt(value);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// MSL Waits
//
//////////////////////////////////////////////////////////////////////

bool TrackManager::mslWait(MslWait* wait, MslContextError* error)
{
    bool success = false;

    int trackNumber = wait->track;
    if (trackNumber <= 0) {
        trackNumber = getFocusedTrackIndex() + 1;
        // assuming it's okay to trash this, we have similar issues
        // with MslAction and MslQuery
        // Mobius can handle this without it, but the generic handler can't
        wait->track = trackNumber;
    }

    LogicalTrack* lt = getLogicalTrack(trackNumber);
    if (lt != nullptr) {
        // can now go through generic handling down to two frame and event
        // wait interfaces
        //if (lt->getType() == Session::TypeAudio)
        //success = audioEngine->mslWait(wait, error);
        //else
        success = mslHandler.mslWait(lt, wait, error);
    }
    
    if (!success) {
        Trace(1, "TrackManager: MslWait scheduling failed");
    }
    else {
        Trace(2, "TrackManager: MslWait scheduled at frame %d", wait->coreEventFrame);
    }
    
    return success;
}

/**
 * Called when an internal event that had an MslWait has finished
 */
void TrackManager::finishWait(MslWait* wait, bool canceled)
{
    kernel->finishWait(wait, canceled);
}

//////////////////////////////////////////////////////////////////////
//
// Content Transfer
//
//////////////////////////////////////////////////////////////////////

/**
 * This may be called from the main menu, or drag and drop.
 * The track number is 1 based and expected to be within the range
 * of MIDI tracks.  If it isn't, the UI didn't do it's job so abandon
 * the sequence so we don't accidentally trash something.
 *
 * Violates the usual track interfaces since it can only go to a MIDI track.
 * Don't like this.
 */
void TrackManager::loadLoop(MidiSequence* seq, int track, int loop)
{
    LogicalTrack* lt = getLogicalTrack(track);
    MidiTrack* mt = lt->getMidiTrack();
    if (mt == nullptr) {
        Trace(1, "TrackManager::loadLoop Invalid track number %d", track);
        pools.reclaim(seq);
    }
    else {
        mt->loadLoop(seq, loop);
    }
}

/**
 * Experimental drag-and-drop file saver
 */
juce::StringArray TrackManager::saveLoop(int trackNumber, int loopNumber, juce::File& file)
{
    juce::StringArray errors;

    LogicalTrack* lt = getLogicalTrack(trackNumber);
    MidiTrack* mt = lt->getMidiTrack();
    if (mt == nullptr) {
        Trace(1, "TrackManager::saveLoop Invalid track number %d", trackNumber);
    }
    else {
        Trace(1, "TrackManager::saveLoop Not implemented");
        (void)loopNumber;
        (void)file;
    }

    return errors;
}    

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

void TrackManager::refreshState(SystemState* state)
{
    int audioTracks = 0;
    int midiTracks = 0;
    for (auto track : tracks) {
        int trackIndex = track->getNumber() - 1;
        if (trackIndex >= state->tracks.size()) {
            // this should have been pre-sized
            Trace(1, "TrackManager: Not enough SystemState tracks");
        }
        else {
            TrackState* tstate = state->tracks[trackIndex];
            if (tstate != nullptr) {
                // make sure this is set properly the BaseTracks may forget
                tstate->number = track->getNumber();
                track->refreshState(tstate);
            }
        }

        if (track->getType() == Session::TypeMidi)
          midiTracks++;
        else
          audioTracks++;
    }

    state->audioTracks = audioTracks;
    state->midiTracks = midiTracks;

    // both types support the new focused state for events
    // this must have been set by Supervisor when the SystemState
    // was passed down for refresh
    if (state->focusedTrack > 0) {
        LogicalTrack* lt = getLogicalTrack(state->focusedTrack);
        lt->refreshFocusedState(&(state->focusedState));
    }
}

void TrackManager::refreshPriorityState(PriorityState* state)
{
    // don't really have anything to say yet
    for (auto track : tracks) {
        track->refreshPriorityState(state);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
