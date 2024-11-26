/**
 * Encapsulates code related to the core Mobius integration with MSL.
 */

#pragma once

//#include "MobiusMslVariableHandler.h"

class MobiusMslHandler
{
  public:

    MobiusMslHandler(class Mobius* m);
    ~MobiusMslHandler();

    //bool mslQuery(class MslQuery* query);
    bool scheduleWaitFrame(class MslWait* wait, int frame);
    bool scheduleWaitEvent(class MslWait* wait);

  private:

    class Mobius* mobius = nullptr;
    //MobiusMslVariableHandler variables;

    class Track* getWaitTarget(class MslWait* wait);
    
};

