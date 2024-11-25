/**
 * An adapter class that implements AbstractTrack so the old
 * Track objects can live in the new world with MidiTrack
 * and the new track architecture.
 *
 */

#pragma once

#include "../../model/MobiusState.h"

#include "TrackProperties.h"
#include "BaseTrack.h"
#incldue "MslTrack.h"

class MobiusLooperTrack : public BaseTrack, public MslTrack
{
  public:

    MobiusLooperTrack(class Mobius* m, class Track* t);
    ~MobiusLooperTrack();

    //
    // BaseTrack
    //

    void loadSssion(class Session::Track* def) override;
    void doAction(class UIAction* a) override;
    bool doQuery(class Query* q) override;
    void processAudioStream(class MobiusAudioStream* stream) override;
    void midiEvent(class MidiEvent* e) override;
    void getTrackProperties(class TrackProperties& props) override;
    void trackNotification(NotificationId notification, TrackProperties& props) override;
    int getGroup() override;
    bool isFocused() override;
    void refreshPriorityState(class MobiusState::Track* tstate) override;
    void refreshState(class MobiusState::Track* tstate) override;
    void dump(class StructureDumper& d) override;

    //
    // MslTrack
    //
    
    class TrackEventList* getEventList() override;
    bool scheduleWaitFrame(class MslWait* w, int frame) override;
    bool scheduleWaitEvent(class MslWait* w) override;

    int getSubcycleFrames() override;
    int getCycleFrames() override;
    int getLoopFrames() override;
    int getFrame() override;
    float getRate() override;
    int getLoopCount() override;
    int getLoopIndex() override;
    int getCycles() override;
    int getSubcycles() override;
    MobiusState::Mode getMode() override;
    bool isPaused() override;

  private:

    class Mobius* mobius = nullptr;
    class Track* track = nullptr;
};
