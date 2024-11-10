/**
 * The Notifier receives callbacks from internal track implementations when interesting
 * things happen.
 *
 * For simplicity, I'm allowing this to have direct dependencies on the internal
 * components that need to touch it: mobius/Track, mobius/Loop, midi/MidiTracker, midi/MidiTrack,
 * midi/TrackScheduler.
 *
 * It would be cleaner if we had some abstract virtual classes to hide those details.
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/Session.h"
#include "../model/Symbol.h"
#include "../model/ScriptProperties.h"
#include "../script/MslEnvironment.h"

#include "MobiusKernel.h"
#include "MobiusInterface.h"

#include "Notification.h"
#include "MobiusPools.h"
#include "TrackProperties.h"

#include "core/Track.h"
#include "core/Loop.h"
#include "core/Mode.h"

#include "Notifier.h"

Notifier::Notifier()
{
    int maxTracks = 100;
    // does this even work?  I guess it does
    for (int i = 0 ; i < maxTracks ; i++) {
        listeners.add(juce::Array<TrackListener*>());
    }

    // may as well do this too
    for (int i = 0 ; i < maxTracks ; i++) {
        juce::Array<TrackListener*>& larray = listeners.getReference(i);
        larray.ensureStorageAllocated(4);
    }
}

Notifier::~Notifier()
{
    flush();
}

void Notifier::initialize(MobiusKernel* k)
{
    kernel = k;
    scriptenv = k->getContainer()->getMslEnvironment();
    symbols = k->getContainer()->getSymbols();
    pool = k->getPools();
}

/**
 * Unfortunate initialization ordering issues around the event script.
 * MobiusShell/Kernel/Notifier gets initialized before the script environment
 * so if we try to locate the symbol now it won't be found.  Have to save the name
 * and look it up the next time an event is received.
 */
void Notifier::configure(Session* s)
{
    scriptSymbol = nullptr;
    scriptName = juce::String(s->getString("eventScript"));
}

//////////////////////////////////////////////////////////////////////
//
// Mobius Core Notifications
//
//////////////////////////////////////////////////////////////////////

void Notifier::notify(Loop* loop, NotificationId id)
{
    notify(loop->getTrack(), id);
}

/**
 * Simple non-payload notifier
 * This started life with MIDI track listeners.
 *
 * With event scripts, some sort of script manager could also
 * be implemented as a listener, but I'm starting simple and having
 * Notifier deal with that.
 */
void Notifier::notify(Track* track, NotificationId id)
{
    int trackNumber = track->getDisplayNumber();
    //Trace(2, "Notifier: Received notification %d for track %d",
    //(int)id, track->getDisplayNumber());

    TrackProperties props;
    props.number = trackNumber;
    props.frames = track->getFrames();
    props.cycles = track->getCycles();
    props.currentFrame = (int)(track->getFrame());
    
    // handle the listeners
    if (trackNumber < 0 || trackNumber >= listeners.size()) {
        Trace(1, "Notififier: Listener array is fucked");
    }
    else {
        juce::Array<TrackListener*>& larray = listeners.getReference(trackNumber);
        if (larray.size() > 0) {

            for (auto l : larray) {
                l->trackNotification(id, props);
            }
        }
    }

    NotificationPayload payload;
    notifyScript(id, props, payload);
}

/**
 * This can be called from the outside with a partially constructed
 * TrackProperties object that has more than just the track state.
 */
void Notifier::notify(Track* track, NotificationId id, TrackProperties& props)
{
    int trackNumber = track->getDisplayNumber();
    //Trace(2, "Notifier: Received notification %d for track %d",
    //(int)id, track->getDisplayNumber());

    // trust that the track info has already been filled in or do ot for the caller?
    props.number = trackNumber;
    props.frames = track->getFrames();
    props.cycles = track->getCycles();
    props.currentFrame = (int)(track->getFrame());

    if (trackNumber < 0 || trackNumber >= listeners.size()) {
        Trace(1, "Notififier: Listener array is fucked");
    }
    else {
        juce::Array<TrackListener*>& larray = listeners.getReference(trackNumber);
        if (larray.size() > 0) {

            for (auto l : larray) {
                l->trackNotification(id, props);
            }
        }
    }

    NotificationPayload payload;
    notifyScript(id, props, payload);
}

void Notifier::notify(Loop* loop, NotificationId id, NotificationPayload& payload)
{
    notify(loop->getTrack(), id, payload);
}

void Notifier::notify(Track* track, NotificationId id, NotificationPayload& payload)
{
    int trackNumber = track->getDisplayNumber();
    TrackProperties props;
    props.number = trackNumber;
    props.frames = track->getFrames();
    props.cycles = track->getCycles();
    props.currentFrame = (int)(track->getFrame());
            
    // the older ones that don't use a payload won't have listeners and listeners
    // aren't prepared to accept a payload, add listeners later

    notifyScript(id, props, payload);
}

//////////////////////////////////////////////////////////////////////
//
// Scripts
//
//////////////////////////////////////////////////////////////////////

/**
 * Let's start by doing these synchronously rather than messing with
 * the Notification queue.
 *
 * The usual process for calling scripts is with UIAction with a Symbol containing
 * ScriptProperties and the MslLinkage.  But UIAction and the action interface can't
 * handle complex variable argument lists.
 *
 * We'll bypass all that and call MslEnvironment directly.  The Symbol exists here only
 * to track changes to the MslLinkage which may be replaced as scripts are loaded and unloaded.
 */
void Notifier::notifyScript(NotificationId id, TrackProperties& props, NotificationPayload& payload)
{
    // see configure() for why we have to defer this
    if (scriptSymbol == nullptr && scriptName.length() > 0) {
        scriptSymbol = symbols->find(scriptName);
        if (scriptSymbol == nullptr) {
            Trace(1, "Notifier: Configured script not found %s",
                  scriptName.toUTF8());
            // to prevent this from happening every time, could null this but
            // then it won't heal itself after they load the script
            //scriptSymbol = "";
        }
    }

    if (scriptSymbol != nullptr) {
        ScriptProperties* sprops =  scriptSymbol->script.get();
        if (sprops == nullptr) {
            Trace(1, "Notifier: Notification script is not a script symbol: %s",
                  scriptSymbol->getName());
        }
        else if (sprops->mslLinkage == nullptr) {
            Trace(1, "Notifier: Notification script is not an MSL script: %s",
                  scriptSymbol->getName());
        }
        else {
            const char* typeName = mapNotificationId(id);
            if (strlen(typeName) == 0) {
                // this means don't pass it
            }
            else {
                MslRequest req;
                req.linkage = sprops->mslLinkage;

                // the arglist we have to deal with, would be kind of nice if we didn't
                // also would be nice to have keyword arguments here rather than positional
                // signature assumption: script(id, trackNumber, argstring)

                // these are a linked list so do them in reverse order
                MslValue* arguments = nullptr;
                
                MslValue* v = scriptenv->allocValue();
                mapPayloadArgument(payload, v);
                v->next = arguments;
                arguments = v;

                v = scriptenv->allocValue();
                v->setInt(props.number);
                v->next = arguments;
                arguments = v;

                v = scriptenv->allocValue();
                v->setString(typeName);
                v->next = arguments;
                arguments = v;

                req.arguments = arguments;
                scriptenv->request(kernel, &req);

                // no meaningful return value, but could have errors
                if (req.error.hasError())
                  Trace(1, "Notifier: Script error %s", req.error.error);

                scriptenv->free(arguments);
            
            }
        }
    }
}

/**
 * Map a NotificationId into a name to pass into the event script.
 * If this returns empty string, it should not be passed.
 */
const char* Notifier::mapNotificationId(NotificationId id)
{
    const char* name = "";

    switch (id) {
        case NotificationReset: name = "Reset"; break;
        case NotificationRecordStart: name="RecordStart"; break;
        case NotificationRecordEnd: name = "RecordEnd"; break;
        case NotificationMuteStart: name = "MuteStart"; break;
        case NotificationMuteEnd: name = "MuteEnd"; break;
        case NotificationModeStart: name = "ModeStart"; break;
        case NotificationModeEnd: name = "ModeEnd"; break;
        case NotificationLoopStart: name = "LoopStart"; break;
        case NotificationLoopCycle: name = "LoopCycle"; break;

            // this may be too noisy?
            //case NotificationLoopSubcycle: name = "LoopSubcycle"; break;
            
        default: break;
    }
    return name;
}

/**
 * Map the NotificationPayload arguments into a single string value
 * Only handling mode right now, obviously will need more.
 */
void Notifier::mapPayloadArgument(NotificationPayload& payload, MslValue* arg)
{
    if (payload.mode != nullptr) {
        arg->setString(payload.mode->getName());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Listeners
//
//////////////////////////////////////////////////////////////////////

void Notifier::addTrackListener(int trackNumber, TrackListener* l)
{
    if (trackNumber >= 0 && trackNumber < listeners.size()) {
        juce::Array<TrackListener*>& larray = listeners.getReference(trackNumber);
        if (!larray.contains(l))
          larray.add(l);
    }
    else {
        Trace(1, "Notififier: Listener array is fucked");
    }
}

void Notifier::removeTrackListener(int trackNumber, TrackListener* l)
{
    if (trackNumber >= 0 && trackNumber < listeners.size()) {
        juce::Array<TrackListener*>& larray = listeners.getReference(trackNumber);
        larray.removeAllInstancesOf(l);
    }
    else {
        Trace(1, "Notififier: Listener array is fucked");
    }
}

//////////////////////////////////////////////////////////////////////
//
// The Notification Queue
//
//////////////////////////////////////////////////////////////////////

Notification* Notifier::alloc()
{
    return pool->newNotification();
}

void Notifier::add(Notification* n)
{
    if (head == nullptr) {
        head = n;
        if (tail != nullptr)
          Trace(1, "Notifier: Lingering tail");
    }
    else if (tail != nullptr) {
        tail->next = n;
    }
    else {
        Trace(1, "Notifier: Missing tail");
        tail = head;
        while (tail->next != nullptr) tail = tail->next;
    }
    tail = n;
}

void Notifier::flush()
{
    while (head != nullptr) {
        Notification* next = head->next;
        head->next = nullptr;
        pool->checkin(head);
        head = next;
    }
    tail = nullptr;
}

/**
 * Process any notifications allowed to happen at the end of an audio block.
 * This also flushes all queued notifications after processing.
 */
void Notifier::afterBlock()
{
    flush();
}

void Notifier::afterEvent(int track)
{
    (void)track;
}

void Notifier::afterTrack(int track)
{
    (void)track;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
