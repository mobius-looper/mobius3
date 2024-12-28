/**
 * A major system component that provides services related to synchronization.
 * This is a new and evolving refactoring of parts of the old mobius/core/Synchronizer.
 *
 * It is an AudioThread aka Kernel component and must be accessed from the UI
 * trough Actions.
 *
 * Among the services provided are:
 *
 *    - a "Transport" for an internal sync generator with a tempo, time signature
 *      and a timeline that may be advanced with Start/Stop/Pause commands
 *    - receiption of MIDI input clocks and conversion into an internal Pulse model
 *    - reception of plugin host synchronization state with Pulse conversion
 *    - generation of MIDI output clocks to send to external applications and devices
 *    - generation of an audible Metronome
 */

#pragma once

#include "Transport.h"
#include "Pulse.h"

class SyncMaster
{
  public:
    
    SyncMaster();
    ~SyncMaster();

    void setSampleRate(int rate);

    void loadSession(class Session* s);
    
    void doAction(class UIAction* a);
    bool doQuery(class Query* q);
    void advance(int frames);

    void refreshState(class SyncMasterState* s);
    void refreshPriorityState(class PriorityState* s);

    // direct access for Synchronizer
    Transport* getTransport() {
        return &transport;
    }

    // this is what core Synchronizer uses to get internal sync pulses
    void getTransportPulse(class Pulse& p);

  private:

    class Provider* provider = nullptr;
    Transport transport;
    // why can't this be in Transport?
    Pulse transportPulse;
    
    void doStop(class UIAction* a);
    void doStart(class UIAction* a);
    void doTempo(class UIAction* a);
    void doBeatsPerBar(class UIAction* a);

};
