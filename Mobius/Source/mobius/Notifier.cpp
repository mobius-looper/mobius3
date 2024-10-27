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
#include "Notification.h"
#include "MobiusPools.h"

#include "core/Track.h"
#include "core/Loop.h"
#include "midi/MidiTracker.h"
#include "midi/MidiTrack.h"

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

void Notifier::setPool(class MobiusPools* p)
{
    pool = p;
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

void Notifier::notify(Track* track, NotificationId id)
{
    Trace(2, "Notifier: Received notification %d for track %d",
          (int)id, track->getDisplayNumber());

    int trackNumber = track->getDisplayNumber();
    if (trackNumber < 0 || trackNumber >= listeners.size()) {
        Trace(1, "Notififier: Listener array is fucked");
    }
    else {
        juce::Array<TrackListener*>& larray = listeners.getReference(trackNumber);
        if (larray.size() > 0) {

            TrackProperties props;
            props.number = trackNumber;
            props.frames = track->getFrames();
            props.cycles = track->getCycles();
            props.currentFrame = track->getFrame();
            
            for (auto l : larray) {
                l->trackNotification(id, props);
            }
        }
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
