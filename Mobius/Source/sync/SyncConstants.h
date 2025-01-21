/**
 * A set of enumerations for things in the synchronization model.
 */

#pragma once

/**
 * The sync source defines a fundamental type of synchronization provider.
 * It may be used to request synchronization from a provider, and to identify
 * where synchronization pulses came from.
 * 
 * A source may either be Internal or External.
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
     * Also referred to as a "leader"
     */
    SyncSourceTrack,

    /**
     * The plugin host application.
     */
    SyncSourceHost,

    /**
     * External MIDI clocks.
     */
    SyncSourceMidi,

    /**
     * A special value used in track configuration indiciating that this
     * track wishes to be the TransportMaster.
     */
    SyncSourceMaster

} SyncSource;

/**
 * The sync unit defines the granularity of a pulse from a sync source.
 * The smallest unit is Beat.  Bars are made up of multiiple beats, and
 * loops are made up of multiple bars.
 *  
 * None is used in a few contexts may indiciate falling back to a default or
 * global unit.  In others it is nonsensical and will default to Beat.
 */
typedef enum {

    SyncUnitNone,
    SyncUnitBeat,
    SyncUnitBar,
    SyncUnitLoop
    
} SyncUnit;

typedef enum {

    TrackUnitSubcycle,
    TrackUnitCycle,
    TrackUnitLoop

} TrackSyncUnit;
    
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
