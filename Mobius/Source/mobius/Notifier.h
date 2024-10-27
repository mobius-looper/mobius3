/**
 * The class that manages the distribution of notification events from the two main kernel
 * components: Mobius (audio) and MidiTracker.
 *
 * There are two possible receivers for a notification.  If the track sending the notification
 * has any TrackListeners, they will be immediately informed.
 *
 * If there are any asynchronous notification watchers, a Notification object is queued
 * and processed later.
 *
 */

#pragma once

#include "JuceHeader.h"

#include "TrackListener.h"

class Notifier
{
  public:

    Notifier();
    ~Notifier();
    void setPool(class MobiusPools* p);


    //
    // New interface used for MIDI followers
    //
    void addTrackListener(int trackNumber, TrackListener* l);
    void removeTrackListener(int trackNumber, TrackListener* l);

    void notify(class Loop* loop, NotificationId id);
    void notify(class Track* track, NotificationId id);

    //
    // Old interface used for the initial prototype
    //
    
    class Notification* alloc();
    void add(class Notification* n);
    

    void afterEvent(int track);
    void afterTrack(int track);
    void afterBlock();

  private:

    void flush();

    class MobiusPools* pool = nullptr;
    class Notification* head = nullptr;
    class Notification* tail = nullptr;

    // will need a better way to do this
    juce::Array<juce::Array<TrackListener*>> listeners;

};

