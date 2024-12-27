/**
 * An adapter class that implements BaseTrack so the old
 * Track objects can live in the new world with MidiTrack
 * and the new track architecture.
 *
 */

#pragma once

#include "../../model/MobiusState.h"

#include "TrackProperties.h"
#include "BaseTrack.h"
#include "MslTrack.h"

class MobiusLooperTrack : public BaseTrack, public MslTrack
{
  public:

    MobiusLooperTrack(class TrackManager* tm, class LogicalTrack*lt,
                      class Mobius* m, class Track* t);
    ~MobiusLooperTrack();

    //
    // BaseTrack
    //

    void loadSession(class Session::Track* def) override;
    void doAction(class UIAction* a) override;
    bool doQuery(class Query* q) override;
    void processAudioStream(class MobiusAudioStream* stream) override;
    void midiEvent(class MidiEvent* e) override;
    void getTrackProperties(class TrackProperties& props) override;
    void trackNotification(NotificationId notification, TrackProperties& props) override;
    int getGroup() override;
    bool isFocused() override;
    void refreshState(class TrackState* state) override;
    void refreshPriorityState(class MobiusState::Track* tstate) override;
    void refreshDynamicState(class DynamicState* state) override;
    void refreshState(class MobiusState::Track* tstate) override;
    void dump(class StructureDumper& d) override;
    class MslTrack* getMslTrack() override;

    //
    // MslTrack
    //
    
    int getSubcycleFrames() override;
    int getCycleFrames() override;
    int getFrames() override;
    int getFrame() override;
    float getRate() override;

    bool scheduleWaitFrame(class MslWait* w, int frame) override;
    bool scheduleWaitEvent(class MslWait* w) override;

    int getLoopCount() override;
    int getLoopIndex() override;
    int getCycles() override;
    int getSubcycles() override;
    MobiusState::Mode getMode() override;
    bool isPaused() override;
    bool isMuted() override;
    bool isOverdub() override;
    
  private:

    class Mobius* mobius = nullptr;
    class Track* track = nullptr;
};
