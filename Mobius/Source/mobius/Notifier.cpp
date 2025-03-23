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
#include "../script/MslBinding.h"
#include "../script/MslValue.h"
#include "../script/MslResult.h"
#include "../script/MslError.h"

#include "MobiusKernel.h"
#include "MobiusInterface.h"

#include "Notification.h"
#include "MobiusPools.h"
#include "track/TrackProperties.h"
#include "track/TrackManager.h"
#include "track/LogicalTrack.h"

#include "core/Track.h"
#include "core/Loop.h"
#include "core/Mode.h"

#include "Notifier.h"

Notifier::Notifier()
{
}

Notifier::~Notifier()
{
    flush();
}

void Notifier::initialize(MobiusKernel* k, TrackManager* tm)
{
    kernel = k;
    trackManager = tm;
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
// todo: Dislike having a dependency on the Track model, would be
// better to have an interface.
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
    LogicalTrack* lt = track->getLogicalTrack();
    if (lt == nullptr) {
        Trace(1, "Notifier: Track without LogicalTrack");
    }
    else {
        TrackProperties props;
        props.number = lt->getNumber();
        props.frames = track->getFrames();
        props.cycles = track->getCycles();
        props.currentFrame = (int)(track->getFrame());

        // inform the track listeners
        lt->notifyListeners(id, props);

        // and any scripts
        NotificationPayload payload;
        notifyScript(id, props, payload);
    }
}

/**
 * This can be called from the outside with a partially constructed
 * TrackProperties object that has more than just the track state.
 */
void Notifier::notify(Track* track, NotificationId id, TrackProperties& props)
{
    LogicalTrack* lt = track->getLogicalTrack();
    if (lt == nullptr) {
        Trace(1, "Notifier: Track without LogicalTrack");
    }
    else {
        // trust that the track info has already been filled in or do ot for the caller?
        props.number = lt->getNumber();
        props.frames = track->getFrames();
        props.cycles = track->getCycles();
        props.currentFrame = (int)(track->getFrame());

        lt->notifyListeners(id, props);

        NotificationPayload payload;
        notifyScript(id, props, payload);
    }
}

void Notifier::notify(Loop* loop, NotificationId id, NotificationPayload& payload)
{
    notify(loop->getTrack(), id, payload);
}

void Notifier::notify(Track* track, NotificationId id, NotificationPayload& payload)
{
    LogicalTrack* lt = track->getLogicalTrack();
    if (lt == nullptr) {
        Trace(1, "Notifier: Track without LogicalTrack");
    }
    else {
        TrackProperties props;
        props.number = lt->getNumber();
        props.frames = track->getFrames();
        props.cycles = track->getCycles();
        props.currentFrame = (int)(track->getFrame());
            
        // the older ones that don't use a payload won't have listeners and listeners
        // aren't prepared to accept a payload, add listeners later

        notifyScript(id, props, payload);
    }
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Tracks
//
// Basically the same shit but for Midi tracks
//
// Revisit this when the dust settles so we can share some of this
//
//////////////////////////////////////////////////////////////////////

void Notifier::notify(LogicalTrack* lt, NotificationId id)
{
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
                // todo: Need an MslRequestBuilder like we do for MslResult
                MslRequest req;
                req.linkage = sprops->mslLinkage;

                // the signature is script(eventType, eventTrack, eventMode)
                // these can be referenced both by name and by $x positions (the very first
                // release of this) so keep these in order

                // argument 1: type
                MslBinding* arguments = makeBinding("eventType", typeName);
                MslBinding* last = arguments;

                // argument 2: track
                MslBinding* arg = makeBinding("eventTrack", props.number);
                last->next = arg;
                last = arg;

                // argument 3: mode name
                // todo: probably will need different names here for the different
                // events, or genericize this as "eventData"
                if (payload.mode != nullptr) {
                    arg = makeBinding("eventMode", payload.mode->getName());
                }
                else {
                    // if this isn't a mode change event, pass a null argument
                    // just to avoid unresolved $3 references in the script
                    arg = makeBinding("eventMode", nullptr);
                }
                last->next = arg;
                last = arg;

                // ownership if the arguments is taken by the environment
                // the request stays with the caller
                req.bindings = arguments;
                
                MslResult* res = scriptenv->request(kernel, &req);
                if (res != nullptr) {
                    // no meaningful return value, but could have errors
                    if (res->errors != nullptr)
                      Trace(1, "Notifier: Script error %s", res->errors->details);
                    scriptenv->free(res);
                }
            }
        }
    }
}

MslBinding* Notifier::makeBinding(const char* name, const char* value)
{
    MslBinding* b = scriptenv->allocBinding();
    b->setName(name);
    MslValue* v = scriptenv->allocValue();
    v->setString(value);
    b->value = v;
    return b;
}

MslBinding* Notifier::makeBinding(const char* name, int value)
{
    MslBinding* b = scriptenv->allocBinding();
    b->setName(name);
    MslValue* v = scriptenv->allocValue();
    v->setInt(value);
    b->value = v;
    return b;
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

//////////////////////////////////////////////////////////////////////
//
// The Notification Queue
//
// This is not actually used.  Probably won't be but keep it around
// for awhile.
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
