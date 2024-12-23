/**
 * A hidden track that acts as a basis for metronome synchronization.
 */

#pragma once

#include "BaseTrack.h"

class MetronomeTrack : public BaseTrack
{
  public:
    
    MetronomeTrack(class TrackManager* tm, class LogicalTrack* lt);
    ~MetronomeTrack();
    
    void loadSession(class Session::Track* def) override;
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
    class MslTrack* getMslTrack() override;
    
  private:
    
    void doStop(class UIAction* a);
    void doStart(class UIAction* a);
    void doTempo(class UIAction* a);
    void doBeatsPerBar(class UIAction* a);

    void advance(int frames);

    int calcTempoLength(float tempo, int bpb);
    
    int beatsPerBar = 0;

};

