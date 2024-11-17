/**
 * Encapsulates code related to the core Mobius integration with MSL.
 */

#pragma once

#include "MobiusMslVariableHandler.h"

class MobiusMslHandler
{
  public:

    MobiusMslHandler(class Mobius* m);
    ~MobiusMslHandler();

    bool mslQuery(class MslQuery* query);
    bool mslWait(class MslWait* wait, class MslContextError* error);

  private:

    class Mobius* mobius = nullptr;
    MobiusMslVariableHandler variables;

    // new MSL support
    void scheduleWaitAtFrame(class MslWait* wait, class Track* track, int frame);
    class Track* getWaitTarget(class MslWait* wait);
    bool scheduleDurationWait(class MslWait* wait);
    int calculateDurationFrame(class MslWait* wait, class Track* t);
    int getMsecFrames(class Track* t, long msecs);
    bool scheduleLocationWait(class MslWait* wait);
    int calculateLocationFrame(class MslWait* wait, class Track* track);
    bool scheduleEventWait(class MslWait* wait);

};

