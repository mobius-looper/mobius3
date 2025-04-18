/**
 * An adapter class that implements BaseTrack so the old
 * Track objects can live in the new world with MidiTrack
 * and the new track architecture.
 *
 */

#pragma once

#include "../../model/TrackState.h"

#include "TrackProperties.h"
#include "BaseTrack.h"
#include "MslTrack.h"

class MobiusLooperTrack : public BaseTrack, public MslTrack
{
  public:

    MobiusLooperTrack(class TrackManager* tm, class LogicalTrack*lt,
                      class Mobius* m, class Track* t);

    MobiusLooperTrack(class TrackManager* tm, class LogicalTrack*lt);
    
    ~MobiusLooperTrack();

    class Track* getCoreTrack();
    void setCoreTrack(class Mobius* m, class Track* t);
    int getCoreTrackNumber();
    
    //
    // BaseTrack
    //

    void refreshParameters() override;
    void doAction(class UIAction* a) override;
    bool doQuery(class Query* q) override;
    void processAudioStream(class MobiusAudioStream* stream) override;
    void midiEvent(class MidiEvent* e) override;
    void getTrackProperties(class TrackProperties& props) override;
    void trackNotification(NotificationId notification, TrackProperties& props) override;
    void refreshState(class TrackState* state) override;
    void refreshPriorityState(class PriorityState* state) override;
    void refreshFocusedState(class FocusedTrackState* state) override;
    void dump(class StructureDumper& d) override;
    class MslTrack* getMslTrack() override;
    void syncEvent(class SyncEvent* e) override;
    int getSyncLength() override;
    int getSyncLocation() override;
    int scheduleFollowerEvent(QuantizeMode q, int follower, int eventId) override;

    bool scheduleWait(class TrackWait& wait) override;
    void cancelWait(class TrackWait& wait) override;
    void finishWait(class TrackWait& wait) override;

    void gatherContent(class TrackContent* content) override;
    void loadContent(class TrackContent* content, class TrackContent::Track* src) override;
    
    //
    // MslTrack
    //

    bool isRecorded() override;
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
    TrackState::Mode getMode() override;
    bool isPaused() override;
    bool isMuted() override;
    bool isOverdub() override;
    
  private:

    class Mobius* mobius = nullptr;
    class Track* track = nullptr;
};
