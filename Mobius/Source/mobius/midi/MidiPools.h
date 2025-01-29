/**
 * Packages the various object pools related to MIDI processing.
 *
 * This makes it easier for the different classes to do things that require
 * pooled objects without having to pass many different pools around during initialization.
 *
 * The pool also provides a set of convenience methods for allocating, clearing, and
 * reclaiming objects.  It isn't required those be used, but it reduces
 * the amount of code callers need when dealing with pooled objects.
 *
 * Each object in the pool is expected to have these methods:
 *
 *      clear(MidiPools* pools)
 *         - returns any objects allocated within this object back to the pool
 *
 *      copy(MidiPools* pools, <class> source)
 *         - makes a copy of another object of the same class using the pools for allocation
 *
 * MidiPools then provides these methods for all pooled object classes:
 *
 *       newFoo       - allocate a new empty object
 *       checkin(Foo) - returns an empty object to the pool but does not empty it
 *       copy(Foo)    - returns a copy of another object
 *       clear(Foo)   - returns objects inside another to the pool but retains the container
 *       reclaim(Foo) - clears the object, then returns the object to the pool
 *
 */

#pragma once

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"
#include "../../model/UIAction.h"

#include "MidiLayer.h"
#include "MidiSegment.h"
#include "MidiFragment.h"

// needs work on where all the pools go, nice to have them in one place
// but then it's kind of a mess
// this one probably can be on it's own or in a TrackPools
//#include "../track/TrackEvent.h"


class MidiPools
{
  public:

    MidiPools();
    ~MidiPools();

    //
    // Pools must be defined in dependency order so pools can return things
    // to pools defined above during destruction
    //

    MidiEventPool midiPool;
    MidiSequencePool sequencePool;
    MidiLayerPool layerPool;
    MidiSegmentPool segmentPool;
    MidiFragmentPool fragmentPool;

    // doesn't belong here
    //TrackEventPool trackEventPool;

    // this one we don't own
    //class UIActionPool* actionPool = nullptr;

    //
    // MidiEvent
    //

    MidiEventPool* getMidiPool() {
        return &midiPool;
    }
    MidiEvent* newEvent() {
        return midiPool.newEvent();
    }
    void checkin(MidiEvent* e) {
        midiPool.checkin(e);
    }
    MidiEvent* copy(MidiEvent* src) {
        if (src == nullptr) return nullptr;
        MidiEvent* neu = midiPool.newEvent();
        neu->copy(src);
        return neu;
    }
    void clear(MidiEvent* src) {
        // events have no content atm
        (void)src;
    }
    void reclaim(MidiEvent* src) {
        clear(src);
        midiPool.checkin(src);
    }
    
    //
    // MidiSequence
    //

    MidiSequence* newSequence() {
        return sequencePool.newSequence();
    }
    void checkin(MidiSequence* s) {
        sequencePool.checkin(s);
    }
    MidiSequence* copy(MidiSequence* src) {
        if (src == nullptr) return nullptr;
        MidiSequence* neu = sequencePool.newSequence();
        neu->copyFrom(&midiPool, src);
        return neu;
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

    //
    // MidiLayer
    //
    
    MidiLayer* newLayer() {
        return layerPool.newLayer();
    }
    void checkin(MidiLayer* l) {
        layerPool.checkin(l);
    }
    
    // layer copy/clear/reclaim is more complex than others

    //
    // MidiSegment
    //
    
    MidiSegment* newSegment() {
        return segmentPool.newSegment();
    }
    void checkin(MidiSegment* l) {
        segmentPool.checkin(l);
    }
    MidiSegment* copy(MidiSegment* src) {
        if (src == nullptr) return nullptr;
        // this one doesn't do an in-place copy, it makes a new one
        MidiSegment* neu = segmentPool.newSegment();
        neu->copyFrom(this, src);
        return neu;
    }
    void clear(MidiSegment* s) {
        if (s != nullptr) s->clear(this);
    }
    void reclaim(MidiSegment* s) {
        if (s != nullptr) {
            clear(s);
            checkin(s);
        }
    }

    //
    // Fragment
    //
    
    MidiFragment* newFragment() {
        return fragmentPool.newFragment();
    }
    void checkin(MidiFragment* f) {
        fragmentPool.checkin(f);
    }
    MidiFragment* copy(MidiFragment* src) {
        if (src == nullptr) return nullptr;
        MidiFragment* neu = fragmentPool.newFragment();
        neu->copy(this, src);
        return neu;
    }
    void clear(MidiFragment* f) {
        if (f != nullptr) f->clear(this);
    }
    void reclaim(MidiFragment* f) {
        if (f != nullptr) {
            clear(f);
            checkin(f);
        }
    }

    //
    // TrackEvent
    // moved to TrackManager
#if 0    
    TrackEvent* newTrackEvent() {
        return trackEventPool.newEvent();
    }
    void checkin(TrackEvent* e) {
        trackEventPool.checkin(e);
    }
#endif    

    // don't need the others atm

    //
    // UIAction
    //

    // free a list of actions
#if 0    
    void reclaim(UIAction* actions) {
        while (actions != nullptr) {
            UIAction* next = actions->next;
            actionPool->checkin(actions);
            actions = next;
        }
    }
#endif
    
  private:

};
