/**
 * An adapter class that implements AbstractTrack so the old
 * Track objects can live in the new world with MidiTrack
 * and the new track architecture.
 *
 */

#pragma once

#include "../track/AbstractTrack.h"
#include "../track/TrackProperties.h"

#include "LooperTrack.h"

class MobiusLooperTrack : public LooperTrack
{
  public:

    MobiusLooperTrack(class Mobius* m, class Track* t);
    ~MobiusLooperTrack();

    //
    // BaseTrack
    //
    
    int getNumber() override;
    void doAction(class UIAction* a) override;
    bool doQuery(class Query* q) override;
    void advance(int newFrames) override;
    void midiEvent(class MidiEvent* e) override;
    int getFrames() override;
    int getFrame() override;
    void getTrackProperties(TrackProperties& props) override;
    void trackNotification(NotificationId notification, TrackProperties& props) override;
    int getGroup() override;
    bool isFocused() override;
    bool scheduleWaitFrame(class MslWait* w, int frame) override;
    bool scheduleWaitEvent(class MslWait* w) override;
    
    //
    // LooperTrack
    //

    MobiusState::Mode getMode() override;
    int getLoopCount() override;
    int getLoopIndex() override;
    int getCycleFrames() override;
    int getCycles() override;
    int getSubcycles() override;
    int getModeStartFrame() override;
    int getModeEndFrame() override;
    int extendRounding() override;

    //
    // None of these are Necessary aince audio tracks do not
    // pass through TrackScheduler to do things
    // really need to factor this out
    //

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
    void toggleFocusLock() override;

    void finishSwitch(int target) override;
    void loopCopy(int previous, bool sound) override;

    bool isPaused() override;
    void startPause() override;
    void finishPause() override;
   
    // simple one-shot actions
    void doParameter(class UIAction* a) override;
    void doPartialReset() override;
    void doReset(bool full) override;
    void doStart() override;
    void doStop() override;
    void doPlay() override;
    void doUndo() override;
    void doRedo() override;
    void doDump() override;
    void doInstantMultiply(int n) override;
    void doInstantDivide(int n) override;
    void doHalfspeed() override;
    void doDoublespeed() override;

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
    void loop() override;

    float getRate() override;
    int getGoalFrames() override;
    void setGoalFrames(int f) override;

    bool isNoReset() override;
    
  private:

    class Mobius* mobius = nullptr;
    class Track* track = nullptr;
};
