/**
 * Get a few enums out of the larger header files to reduce dependencies
 */

#pragma once

/**
 * Enumeration of the contexts an MSL session can be running in.
 * Within Shell, this could be divided into the two UI and Maintenance
 * threads but those are already blocking each other with a Juce MessageLock
 * so it is safe to cross reference things between them.  This would change
 * if other non-blocking threads are added.
 */
typedef enum {
    MslContextNone,
    MslContextKernel,
    MslContextShell
} MslContextId;

/**
 * The state a session can be in
 */
typedef enum {
    MslStateNone,
    MslStateFinished,
    MslStateError,
    MslStateRunning,
    MslStateWaiting,
    MslStateSuspended,
    MslStateTransitioning
} MslSessionState;
