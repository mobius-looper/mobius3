/**
 * Simple model for name/value pairs, aka "objects"
 */

#pragma once

#include <JuceHeader.h>

#include "MslObjectPool.h"

class MslAttribute : public MslPooledObject
{
  public:
    
    MslAttribute();
    ~MslAttribute();
    void poolInit();
    void clear(class MslPools* p);

    MslAttribute* next = nullptr;
    
    // these aren't commonly user defined so have more control over their size
    char name[32];
    void setName(const char* s) {
        strncpy(name, s, sizeof(name));
    }

    class MslValue* getValue() {
        return value;
    }

    void setValue(MslValue* v);
    void copyValue(MslValue& v);
    
  private:

    MslValue* value = nullptr;

};

class MslObject : public MslPooledObject
{
  public:

    MslObject();
    ~MslObject();
    void poolInit();
    void setPools(class MslPools* p);
    void clear();

  private:

    class MslPools* pools = nullptr;

    // until we can find a no-memory HashMap just keep them
    // on a list, son't have any big objects for awhile
    MslAttribute* attributes = nullptr;
};

class MslAttributePool : public MslObjectPool
{
  public:

    MslAttributePool();
    virtual ~MslAttributePool();

    class MslAttribute* newObject();

  protected:
    
    virtual MslPooledObject* alloc() override;
    
};
class MslObjectValuePool : public MslObjectPool
{
  public:

    MslObjectValuePool();
    virtual ~MslObjectValuePool();

    class MslObject* newObject();

  protected:
    
    virtual MslPooledObject* alloc() override;
    
};

