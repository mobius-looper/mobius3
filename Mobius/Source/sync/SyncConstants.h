/**
 * A set of enumerations that define characteristics of synchronization.
 */

#pragma once

/**
 * The sync source defines a fundamental type of synchronization, or where
 * synchronization pulses and sync unit lengths come from.  A source
 * may either be Internal or External.
 */
typedef enum {

    /**
     * There is no synchronization, the track is "freewheeling"
     */
    SyncSourceNone,

    /**
     * The internal transport.
     */
    SyncSourceTransport,

    /**
     * Any internal track.
     */
    SyncSourceTrack,

    /**
     * The plugin host application.
     */
    SyncSourceHost,

    /**
     * External MIDI clocks.
     */
    SyncSourceMidi

} SyncSource;

/**
 * The sync location defines a time within the sync source.
 */

typedef enum {

    /**
     * None means no time is defined, which may indiciate falling
     * back to a default or global location.
     */
    SyncLocationNone,

    SyncLocationBeat,
    SyncLocationBar,
    SyncLocationLoop,

} SyncUnit;

// not liking Location, now it's really a Unit or Granule

// do we need both?

If Source=Track then beat/bar/loop mean subcycle/cycle/loop

    The only time these may mean different things is when QUANTIZING, but couldn't
    that be done with a QuantizeSource which is usually Self, SyncSource, or Leader?

    

    



    

