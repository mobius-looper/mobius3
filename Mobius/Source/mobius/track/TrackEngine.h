/**
 * An abstraction around a manager of tracks with a particular implementation.
 * Initially there are two: audio and midi
 */

#pragma once

#include "../Notification.h"

class TrackEngine
{
  public:

    virtual ~TrackEngine() {}

    /**
     * Initialize the tracks at startup.
     */
    virtual void initialize(int baseNumber, class Session* session) = 0;

    /**
     * Reconfigure the tracks after startup.
     */
    virtual void loadSession(class Session* session) = 0;

    /**
     * Obtain the track with the given number.
     * It is the responsibility of TrackManager to know the number space
     * supported by this engine.
     * Hmm, might be better to have track numbering be managed at a higher level
     * all each engine needs to know is it's local track indexes.  But for diagnostics
     * like each track knowning it's public number.
     */
    virtual class AbstractTrack* getTrack(int number) = 0;

    /**
     * Return a small number of track properties for exchange with other tracks
     * during scheduling.
     */
    virtual class TrackProperties getTrackProperties(int number) = 0;

    /**
     * Handle notificiations from other tracks.
     */
    virtual void trackNotification(NotificationId notification, class TrackProperties& props) = 0;

    /**
     * Handle an incomming MIDI event
     */
    virtual void midiEvent(class MidiEvent* e) = 0;
    
    /**
     * Advance the track contents.
     */
    virtual void processAudioStream(class MobiusAudioStream* stream) = 0;

    /**
     * Get the track group number assigned to a track
     */
    virtual int getTrackGroup(int number) = 0;

    /**
     * Perform a global action on all tracks.
     */
    virtual void doGlobal(class UIAction* a) = 0;

    /**
     * Perform a track specific action.
     */
    virtual void doAction(class UIAction* a) = 0;

    /**
     * Perform a query on a track whose number is in the Query
     */
    virtual bool doQuery(class Query* q) = 0;

    /**
     * Load a MIDI sequence, only for MIDI tracks.
     * Does this need to be in the engine abstraction, or should we just
     * let TrackManager go around it?
     */
    virtual void loadLoop(class MidiSequence* seq, int track, int loop) = 0;

    /**
     * Add state for the tracks under this engine to the consolidated state objecte.
     */
    virtual void refreshState(class MobiusMidiState* state) = 0;

};

        
