/**
 * An adapter class that implements AbstractTrack so the old
 * Track objects can live in the new world with MidiTrack
 * and the new track architecture.
 *
 */

#pragma once

class MobiusTrackWrapper : public AbstractTrack
{
  public:

    MobiusTrackWrapper(class Mobius* m, class Track* t);
    ~MobiusTrackWrapper();

    // AbstractTrack Implementations

    int getNumber() override;
    MobiusState::Mode getMode() override;
    int getLoopCount() override;
    int getLoopIndex() override;
    int getLoopFrames() override;
    int getFrame() override;
    int getCycleFrames() override;
    int getCycles() override;
    int getSubcycles() override;
    int getModeStartFrame() override;
    int getModeEndFrame() override;
    int extendRounding() override;

    // Mode transitions
    
    void startRecord() override;
    void finishRecord() override;

    void startMultiply() override;
    void finishMultiply() override;
    void unroundedMultiply() override;
    
    void startInsert() override;
    int extendInsert() override;
    void finishInsert() override;
    void unroundedInsert() override;

    void toggleOverdub() override;
    void toggleMute() override;
    void toggleReplace() override;

    void finishSwitch(int target) override;
    void loopCopy(int previous, bool sound) override;

    bool isPaused() override;
    void startPause() override;
    void finishPause() override;
    void doStart() override;
    void doStop() override;
   
    // simple one-shot actions
    void doParameter(class UIAction* a) override;
    void doPartialReset() override;
    void doReset(bool full) override;
    void doPlay() override;
    void doUndo() override;
    void doRedo() override;
    void doDump() override;
    void doInstantMultiply(int n) override;
    void doInstantDivide(int n) override;

    // leader stuff
    void leaderReset(class TrackProperties& props) override;
    void leaderRecordStart() override;
    void leaderRecordEnd(class TrackProperties& props) override;
    void leaderMuteStart(class TrackProperties& props) override;
    void leaderMuteEnd(class TrackProperties& props) override;
    void leaderResized(TrackProperties& props) override;
    void leaderMoved(TrackProperties& props) override;
    
    // advance play/record state between events
    bool isExtending() override;
    void advance(int newFrames) override;
    void loop() override;

    float getRate() override;
    //void setRate(float r) override;
    int getGoalFrames() override;
    void setGoalFrames(int f) override;
    
    bool isNoReset() override;
    
    // misc utilities
    void alert(const char* msg) override;

    // emerging interfaces for MslWait and new track architecture
    class TrackEventList* getEventList() override;

  private:

    class Mobius* mobius = nullptr;
    class Track* track = nullptr;
};
