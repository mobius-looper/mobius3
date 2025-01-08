/**
 * Trace utilities for high-speed synchronization events.
 *
 * The format of a sync trace message is:
 *
 *    <now> <timestamp> <event> <details>
 *
 * Where <now> is the system millisecond time at the moment the trace record
 * was added.
 *
 * <timestamp> is the millisecond time associated with an event as it passes through
 * the software layers.
 *
 * <event> is a short text description of what happened
 *
 * <details> is a longer description formatted with arguments.
 */

#pragma once

extern bool SyncTraceEnabled;

class SyncTrace
{
  public:

    // flesh this out, just using the global flag for now

};
    
