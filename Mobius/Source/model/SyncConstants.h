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
 * Alternate sync source when primary is SyncSourceMaster and there
 * is already a master.
 */
typedef enum {

    SyncAlternateTrack,
    SyncAlternateTransport

} SyncSourceAlternate;

/**
 * The sync unit defines the granularity of a pulse from a sync source.
 * The smallest unit is Beat.  Bars are made up of multiiple beats, and
 * loops are made up of multiple bars.
 */
typedef enum {

    SyncUnitBeat,
    SyncUnitBar,
    SyncUnitLoop,

    // This is a hidden value used internally to indiciate that
    // the unit is specified in a different way.  It is not included
    // in the ParameterProperties definition and won't be visible in the UI
    SyncUnitNone
    
} SyncUnit;

/**
 * Similar units for track sync.  This needs to be different enumeration
 * because they have a different session parameter and display differently
 * in the UI.  And they are not always semantically the same.
 * 
 * There are a few conversions between them though so to prevent some code
 * mess it is REQUIRED that the order of these match SyncUnit so
 * we can do simple integer conversion.
 */
typedef enum {

    TrackUnitSubcycle,
    TrackUnitCycle,
    TrackUnitLoop,
    TrackUnitNone

} TrackSyncUnit;
    
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
