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

    bool get(class MslQuery* q);

  private:

    class Mobius* mobius = nullptr;

    class Track* getTrack(class MslQuery* q);
    
    void getLoopCount(class MslQuery* q);
    void getLoopNumber(class MslQuery* q);
    void getLoopFrames(class MslQuery* q);
    void getLoopFrame(class MslQuery* q);
    void getCycleCount(class MslQuery* q);
    void getCycleNumber(class MslQuery* q);
    void getCycleFrames(class MslQuery* q);
    void getCycleFrame(class MslQuery* q);
    void getSubcycleCount(class MslQuery* q);
    void getSubcycleNumber(class MslQuery* q);
    void getSubcycleFrames(class MslQuery* q);
    void getSubcycleFrame(class MslQuery* q);
    void getModeName(class MslQuery* q);
    void getIsRecording(class MslQuery* q);;
    void getInOverdub(class MslQuery* q);
    void getInHalfspeed(class MslQuery* q);
    void getInReverse(class MslQuery* q);
    void getInMute(class MslQuery* q);
    void getInPause(class MslQuery* q);
    void getInRealign(class MslQuery* q);
    void getInReturn(class MslQuery* q);
    void getPlaybackRate(class MslQuery* q);
    void getTrackCount(class MslQuery* q);
    void getActiveTrack(class MslQuery* q);
    void getFocusedTrack(class MslQuery* q);
    void getScopeTrack(class MslQuery* q);
    void getGlobalMute(class MslQuery* q);
    void getTrackSyncMaster(class MslQuery* q);
    void getOutSyncMaster(class MslQuery* q);
    void getSyncTempo(class MslQuery* q);
    void getSyncRawBeat(class MslQuery* q);
    void getSyncBeat(class MslQuery* q);
    void getSyncBar(class MslQuery* q);
    void getBlockFrames(class MslQuery* q);
    void getSampleRate(class MslQuery* q);
    void getSampleFrames(class MslQuery* q);

};
