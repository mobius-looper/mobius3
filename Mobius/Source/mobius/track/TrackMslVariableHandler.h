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
    bool get(class MslQuery* q, class AbstractTrack* t);

  private:

    class MobiusKernel* kernel = nullptr;

    void getLoopCount(class MslQuery* q, class AbstractTrack* t);
    void getLoopNumber(class MslQuery* q, class AbstractTrack* t);
    void getLoopFrames(class MslQuery* q, class AbstractTrack* t);
    void getLoopFrame(class MslQuery* q, class AbstractTrack* t);
    void getCycleCount(class MslQuery* q, class AbstractTrack* t);
    void getCycleNumber(class MslQuery* q, class AbstractTrack* t);
    void getCycleFrames(class MslQuery* q, class AbstractTrack* t);
    void getCycleFrame(class MslQuery* q, class AbstractTrack* t);
    void getSubcycleCount(class MslQuery* q, class AbstractTrack* t);
    void getSubcycleNumber(class MslQuery* q, class AbstractTrack* t);
    void getSubcycleFrames(class MslQuery* q, class AbstractTrack* t);
    void getSubcycleFrame(class MslQuery* q, class AbstractTrack* t);
    void getModeName(class MslQuery* q, class AbstractTrack* t);
    void getIsRecording(class MslQuery* q, class AbstractTrack* t);;
    void getInOverdub(class MslQuery* q, class AbstractTrack* t);
    void getInHalfspeed(class MslQuery* q, class AbstractTrack* t);
    void getInReverse(class MslQuery* q, class AbstractTrack* t);
    void getInMute(class MslQuery* q, class AbstractTrack* t);
    void getInPause(class MslQuery* q, class AbstractTrack* t);
    void getInRealign(class MslQuery* q, class AbstractTrack* t);
    void getInReturn(class MslQuery* q, class AbstractTrack* t);
    void getPlaybackRate(class MslQuery* q, class AbstractTrack* t);
    void getTrackCount(class MslQuery* q, class AbstractTrack* t);
    void getAudioTrackCount(class MslQuery* q, class AbstractTrack* t);
    void getMidiTrackCount(class MslQuery* q, class AbstractTrack* t);
    void getActiveTrack(class MslQuery* q, class AbstractTrack* t);
    void getFocusedTrack(class MslQuery* q, class AbstractTrack* t);
    void getScopeTrack(class MslQuery* q, class AbstractTrack* t);
    void getGlobalMute(class MslQuery* q, class AbstractTrack* t);
    void getTrackSyncMaster(class MslQuery* q, class AbstractTrack* t);
    void getOutSyncMaster(class MslQuery* q, class AbstractTrack* t);
    void getSyncTempo(class MslQuery* q, class AbstractTrack* t);
    void getSyncRawBeat(class MslQuery* q, class AbstractTrack* t);
    void getSyncBeat(class MslQuery* q, class AbstractTrack* t);
    void getSyncBar(class MslQuery* q, class AbstractTrack* t);
    void getBlockFrames(class MslQuery* q, class AbstractTrack* t);
    void getSampleRate(class MslQuery* q, class AbstractTrack* t);
    void getSampleFrames(class MslQuery* q, class AbstractTrack* t);

    int getSubcycleFrames(class AbstractTrack* t);

    
};
