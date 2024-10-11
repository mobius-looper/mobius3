/**
 * Packages the various object pools related to MIDI processing.
 *
 */

#pragma once

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"
#include "MidiLayer.h"
#include "MidiSegment.h"
#include "MidiFragment.h"
#include "TrackEvent.h"


class MidiPools
{
  public:

    MidiPools();
    ~MidiPools();

    // pools to be defined in dependency order so pools can return things
    // to pools defined above during destruction

    MidiEventPool midiPool;
    MidiSequencePool sequencePool;
    MidiLayerPool layerPool;
    MidiSegmentPool segmentPool;
    MidiFragmentPool fragmentPool;
    TrackEventPool eventPool;

    // sometimes more convenient to get it this way
    MidiEventPool* getMidiPool() {
        return &midiPool;
    }
    MidiEvent* newEvent() {
        return midiPool.newEvent();
    }
    void checkin(MidiEvent* e) {
        midiPool.checkin(e);
    }

    MidiSequence* newSequence() {
        return sequencePool.newSequence();
    }
    void checkin(MidiSequence* s) {
        sequencePool.checkin(s);
    }
    void clear(MidiSequence* s) {
        if (s != nullptr) s->clear(&midiPool);
    }
    void reclaim(MidiSequence* s) {
        if (s != nullptr) {
            clear(s);
            checkin(s);
        }
    }

    MidiLayer* newLayer() {
        return layerPool.newLayer();
    }
    void checkin(MidiLayer* l) {
        layerPool.checkin(l);
    }
    
    MidiSegment* newSegment() {
        return segmentPool.newSegment();
    }
    void checkin(MidiSegment* l) {
        segmentPool.checkin(l);
    }
    void reclaim(MidiSegment* s) {
        if (s != nullptr) {
            clear(&(s->prefix));
            checkin(s);
        }
    }
    
    MidiFragment* newFragment() {
        return fragmentPool.newFragment();
    }
    void checkin(MidiFragment* f) {
        fragmentPool.checkin(f);
    }
    void clear(MidiFragment* f) {
        if (f != nullptr) {
            clear(&(f->sequence));
        }
    }
    void reclaim(MidiFragment* f) {
        if (f != nullptr) {
            clear(f);
            checkin(f);
        }
    }

    TrackEvent* newTrackEvent() {
        return eventPool.newEvent();
    }
    void checkin(TrackEvent* e) {
        eventPool.checkin(e);
    }

  private:

};
