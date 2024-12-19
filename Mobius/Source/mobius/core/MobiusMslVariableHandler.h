// update; no longer userd, delete when ready

/**
 * Encapsulation of the gathering of various bits of information
 * to expose as external variable that may be referenced in MSL scripts.
 *
 * This is functionally the same as the old ScriptInternalVariable objects
 * used by MOS scripts, but avoids having a scad of static objects to
 * implement each variable.
 *
 */

#pragma once

class MobiusMslVariableHandler
{
  public:

    MobiusMslVariableHandler(class Mobius* m);
    ~MobiusMslVariableHandler();

    bool get(class MslQuery* q, class Track* t);

  private:

    class Mobius* mobius = nullptr;
    
    void getLoopCount(class MslQuery* q, class Track* t);
    void getLoopNumber(class MslQuery* q, class Track* t);
    void getLoopFrames(class MslQuery* q, class Track* t);
    void getLoopFrame(class MslQuery* q, class Track* t);
    void getCycleCount(class MslQuery* q, class Track* t);
    void getCycleNumber(class MslQuery* q, class Track* t);
    void getCycleFrames(class MslQuery* q, class Track* t);
    void getCycleFrame(class MslQuery* q, class Track* t);
    void getSubcycleCount(class MslQuery* q, class Track* t);
    void getSubcycleNumber(class MslQuery* q, class Track* t);
    void getSubcycleFrames(class MslQuery* q, class Track* t);
    void getSubcycleFrame(class MslQuery* q, class Track* t);
    void getModeName(class MslQuery* q, class Track* t);
    void getIsRecording(class MslQuery* q, class Track* t);;
    void getInOverdub(class MslQuery* q, class Track* t);
    void getInHalfspeed(class MslQuery* q, class Track* t);
    void getInReverse(class MslQuery* q, class Track* t);
    void getInMute(class MslQuery* q, class Track* t);
    void getInPause(class MslQuery* q, class Track* t);
    void getInRealign(class MslQuery* q, class Track* t);
    void getInReturn(class MslQuery* q, class Track* t);
    void getPlaybackRate(class MslQuery* q, class Track* t);
    void getTrackCount(class MslQuery* q, class Track* t);
    void getAudioTrackCount(class MslQuery* q, class Track* t);
    void getMidiTrackCount(class MslQuery* q, class Track* t);
    void getActiveTrack(class MslQuery* q, class Track* t);
    void getFocusedTrack(class MslQuery* q, class Track* t);
    void getScopeTrack(class MslQuery* q, class Track* t);
    void getGlobalMute(class MslQuery* q, class Track* t);
    void getTrackSyncMaster(class MslQuery* q, class Track* t);
    void getOutSyncMaster(class MslQuery* q, class Track* t);
    void getSyncTempo(class MslQuery* q, class Track* t);
    void getSyncRawBeat(class MslQuery* q, class Track* t);
    void getSyncBeat(class MslQuery* q, class Track* t);
    void getSyncBar(class MslQuery* q, class Track* t);
    void getBlockFrames(class MslQuery* q, class Track* t);
    void getSampleRate(class MslQuery* q, class Track* t);
    void getSampleFrames(class MslQuery* q, class Track* t);

};
