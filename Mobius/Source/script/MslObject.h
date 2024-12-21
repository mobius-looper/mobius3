/**
 * Simple model for name/value pairs, aka "objects"
 */

#pragma once

#include <JuceHeader.h>

#include "../model/ObjectPool.h"

class MslAttribute : public PooledObject
{
  public:
    
    MslAttribute();
    ~MslAttribute();
    void poolInit();

    // these aren't commonly user defined so have more control over their size
    char name[32];
    void setName(const char* s) {
        strncpy(name, s, sizeof(name));
    }

    MslValue value;

    MslAttribute* next = nullptr;
    
}

class MslObject
{
  public:

    
        
        



    

    MslObject();
    ~MslObject();

  private:

    // until we can find a no-memory HashMap just keep them
    // on a list, son't have any big objects for awhile
    
