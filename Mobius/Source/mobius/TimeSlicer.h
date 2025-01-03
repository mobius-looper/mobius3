/**
 * The component responsible for slicing each audio block into
 * subsections, and advancing each track to consume those subsections.
 * Slices are made at various points including synchronization events
 * and script wait expirations.  Between each slice, tracks are notified
 * of the events that have taken place.
 *
 * I rather like the name TimeButcher or perhaps the more refined TimeSurgeon
 * but Slicer has more history with computers.
 *
 * Fundamentally as tracks consume
 * and produce blocks of audio content, they may need to stop and perform various
 * operations beyond just recording and playing that content.  Things like starting
 * and stopping a recording may need to be synchronized with events from the outside
 * world like MIDI clocks or Host beats.   Scripts may need to wait for specific
 * locations in the sample stream before doing things.  And tracks may depend on
 * other tracks for timing when actions are performed.
 *
 */

#pragma once

class TimeSlicer
{
  public:

    TimeSlicer(class MobiusKernel* k, class SyncMaster* sm, class TrackManager* tm);
    ~TimeSlicer();

    void processAudioStream(class MobiusAudioStream* stream);

  private:

    class MobiusKernel* kernel = nullptr;
    class SyncMaster* syncMaster = nullptr;
    class TrackManager* trackManager = nullptr;

};


    
