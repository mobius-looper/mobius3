/**
 * Encapsulation of the gathering of various bits of information
 * to expose as external variable that may be referenced in MSL scripts.
 */

#pragma once

#include "../../script/ScriptExternalId.h"

class TrackMslVariableHandler
{
  public:

    TrackMslVariableHandler(class MobiusKernel* k);
    ~TrackMslVariableHandler();

    // should this be abstract or logical?
    bool get(class MslQuery* q, class MslTrack* t);
    bool get(class VarQuery* q, class MslTrack* t);

  private:

    class MobiusKernel* kernel = nullptr;

    bool get(class MslTrack* t, ScriptExternalId id, class MslValue& result);
    
    void getLoopCount(class MslTrack* t, class MslValue& v);
    void getLoopNumber(class MslTrack* t, class MslValue& v);
    void getLoopFrames(class MslTrack* t, class MslValue& v);
    void getLoopFrame(class MslTrack* t, class MslValue& v);
    void getCycleCount(class MslTrack* t, class MslValue& v);
    void getCycleNumber(class MslTrack* t, class MslValue& v);
    void getCycleFrames(class MslTrack* t, class MslValue& v);
    void getCycleFrame(class MslTrack* t, class MslValue& v);
    void getSubcycleCount(class MslTrack* t, class MslValue& v);
    void getSubcycleNumber(class MslTrack* t, class MslValue& v);
    void getSubcycleFrames(class MslTrack* t, class MslValue& v);
    void getSubcycleFrame(class MslTrack* t, class MslValue& v);
    void getModeName(class MslTrack* t, class MslValue& v);
    void getIsRecording(class MslTrack* t, class MslValue& v);;
    void getInOverdub(class MslTrack* t, class MslValue& v);
    void getInHalfspeed(class MslTrack* t, class MslValue& v);
    void getInReverse(class MslTrack* t, class MslValue& v);
    void getInMute(class MslTrack* t, class MslValue& v);
    void getInPause(class MslTrack* t, class MslValue& v);
    void getInRealign(class MslTrack* t, class MslValue& v);
    void getInReturn(class MslTrack* t, class MslValue& v);
    void getPlaybackRate(class MslTrack* t, class MslValue& v);
    void getTrackCount(class MslTrack* t, class MslValue& v);
    void getAudioTrackCount(class MslTrack* t, class MslValue& v);
    void getMidiTrackCount(class MslTrack* t, class MslValue& v);
    void getActiveTrack(class MslTrack* t, class MslValue& v);
    void getFocusedTrackNumber(class MslTrack* t, class MslValue& v);
    void getScopeTrack(class MslTrack* t, class MslValue& v);
    void getGlobalMute(class MslTrack* t, class MslValue& v);
    void getTrackSyncMaster(class MslTrack* t, class MslValue& v);
    void getOutSyncMaster(class MslTrack* t, class MslValue& v);
    void getSyncTempo(class MslTrack* t, class MslValue& v);
    void getSyncRawBeat(class MslTrack* t, class MslValue& v);
    void getSyncBeat(class MslTrack* t, class MslValue& v);
    void getSyncBar(class MslTrack* t, class MslValue& v);
    void getBlockFrames(class MslTrack* t, class MslValue& v);
    void getSampleRate(class MslTrack* t, class MslValue& v);
    void getSampleFrames(class MslTrack* t, class MslValue& v);

    int getSubcycleFrames(class MslTrack* t);
    
};
