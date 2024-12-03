/**
 * Encapsulation of the gathering of various bits of information
 * to expose as external variable that may be referenced in MSL scripts.
 *
 * This parallels MobiusMslVariableHandler which deals with audio tracks
 * and this one deals with MIDI tracks.
 *
 * In time, this should be implemented on top of AbstractTrack
 */

#pragma once

class TrackMslVariableHandler
{
  public:

    TrackMslVariableHandler(class MobiusKernel* k);
    ~TrackMslVariableHandler();

    // should this be abstract or logical?
    bool get(class MslQuery* q, class MslTrack* t);

  private:

    class MobiusKernel* kernel = nullptr;

    void getLoopCount(class MslQuery* q, class MslTrack* t);
    void getLoopNumber(class MslQuery* q, class MslTrack* t);
    void getLoopFrames(class MslQuery* q, class MslTrack* t);
    void getLoopFrame(class MslQuery* q, class MslTrack* t);
    void getCycleCount(class MslQuery* q, class MslTrack* t);
    void getCycleNumber(class MslQuery* q, class MslTrack* t);
    void getCycleFrames(class MslQuery* q, class MslTrack* t);
    void getCycleFrame(class MslQuery* q, class MslTrack* t);
    void getSubcycleCount(class MslQuery* q, class MslTrack* t);
    void getSubcycleNumber(class MslQuery* q, class MslTrack* t);
    void getSubcycleFrames(class MslQuery* q, class MslTrack* t);
    void getSubcycleFrame(class MslQuery* q, class MslTrack* t);
    void getModeName(class MslQuery* q, class MslTrack* t);
    void getIsRecording(class MslQuery* q, class MslTrack* t);;
    void getInOverdub(class MslQuery* q, class MslTrack* t);
    void getInHalfspeed(class MslQuery* q, class MslTrack* t);
    void getInReverse(class MslQuery* q, class MslTrack* t);
    void getInMute(class MslQuery* q, class MslTrack* t);
    void getInPause(class MslQuery* q, class MslTrack* t);
    void getInRealign(class MslQuery* q, class MslTrack* t);
    void getInReturn(class MslQuery* q, class MslTrack* t);
    void getPlaybackRate(class MslQuery* q, class MslTrack* t);
    void getTrackCount(class MslQuery* q, class MslTrack* t);
    void getAudioTrackCount(class MslQuery* q, class MslTrack* t);
    void getMidiTrackCount(class MslQuery* q, class MslTrack* t);
    void getActiveTrack(class MslQuery* q, class MslTrack* t);
    void getFocusedTrackNumber(class MslQuery* q, class MslTrack* t);
    void getScopeTrack(class MslQuery* q, class MslTrack* t);
    void getGlobalMute(class MslQuery* q, class MslTrack* t);
    void getTrackSyncMaster(class MslQuery* q, class MslTrack* t);
    void getOutSyncMaster(class MslQuery* q, class MslTrack* t);
    void getSyncTempo(class MslQuery* q, class MslTrack* t);
    void getSyncRawBeat(class MslQuery* q, class MslTrack* t);
    void getSyncBeat(class MslQuery* q, class MslTrack* t);
    void getSyncBar(class MslQuery* q, class MslTrack* t);
    void getBlockFrames(class MslQuery* q, class MslTrack* t);
    void getSampleRate(class MslQuery* q, class MslTrack* t);
    void getSampleFrames(class MslQuery* q, class MslTrack* t);

    int getSubcycleFrames(class MslTrack* t);

    
};
