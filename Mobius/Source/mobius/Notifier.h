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

#include <JuceHeader.h>

#include "Notification.h"
#include "track/TrackListener.h"

class Notifier
{
  public:

    Notifier();
    ~Notifier();
    
    void initialize(class MobiusKernel* k);
    void configure(class Session* ses);

    //
    // New interface used for MIDI followers
    //
    void addTrackListener(int trackNumber, TrackListener* l);
    void removeTrackListener(int trackNumber, TrackListener* l);

    void notify(class Loop* loop, NotificationId id);
    void notify(class Track* track, NotificationId id);

    // used in cases where the TrackProperties contains other information
    // about what happened, rare and ugly
    void notify(class Track* track, NotificationId id, class TrackProperties& props);

    // newer using a payload, consider replacing the one above with this
    void notify(class Loop* loop, NotificationId id, NotificationPayload& payload);
    void notify(class Track* track, NotificationId id, NotificationPayload& payload);
    
    //
    // Old interface used for the initial prototype
    //
    
    class Notification* alloc();
    void add(class Notification* n);

    void afterEvent(int track);
    void afterTrack(int track);
    void afterBlock();

  private:

    class MobiusKernel* kernel = nullptr;
    class MslEnvironment* scriptenv = nullptr;
    class SymbolTable* symbols = nullptr;
    juce::String scriptName;
    class Symbol* scriptSymbol = nullptr;

    class MobiusPools* pool = nullptr;
    class Notification* head = nullptr;
    class Notification* tail = nullptr;

    // will need a better way to do this
    juce::Array<juce::Array<TrackListener*>> listeners;

    void notifyScript(NotificationId id, TrackProperties& props, NotificationPayload& payload);
    const char* mapNotificationId(NotificationId id);

    class MslBinding* makeBinding(const char* name, const char* value);
    class MslBinding* makeBinding(const char* name, int value);
    
    void flush();

};

