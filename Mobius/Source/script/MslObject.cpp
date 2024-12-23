
#include <JuceHeader.h>

#include "../model/ObjectPool.h"

#include "MslPools.h"
#include "MslValue.h"
#include "MslObject.h"

//////////////////////////////////////////////////////////////////////
//
// Object
//
//////////////////////////////////////////////////////////////////////

MslObject::MslObject()
{
}

MslObject::~MslObject()
{
    // not an OwnedArray so walk it
    while (attributes != nullptr) {
        MslAttribute* next = attributes->next;
        attributes->next = nullptr;
        delete attributes;
        attributes = next;
    }
}

void MslObject::poolInit()
{
    pools = nullptr;
    attributes = nullptr;
}

void MslObject::setPools(MslPools* p)
{
    pools = p;
}

void MslObject::clear()
{
    while (attributes != nullptr) {
        MslAttribute* next = attributes->next;
        attributes->next = nullptr;
        if (pools != nullptr) {
            attributes->clear(pools);
            pools->free(attributes);
        }
        else {
            delete attributes;
        }
        attributes = next;
    }
}

MslObjectValuePool::MslObjectValuePool()
{
    setName("MslObject");
    setObjectSize(sizeof(MslObject));
    fluff();
}

MslObjectValuePool::~MslObjectValuePool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
MslPooledObject* MslObjectValuePool::alloc()
{
    return new MslObject();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
MslObject* MslObjectValuePool::newObject()
{
    return (MslObject*)checkout();
}

//////////////////////////////////////////////////////////////////////
//
// Attribute
//
//////////////////////////////////////////////////////////////////////

MslAttribute::MslAttribute()
{
}

MslAttribute::~MslAttribute()
{
    // todo: clear the value if it contains another MslObject
}

void MslAttribute::poolInit()
{
    strcpy(name, "");
    next = nullptr;
    value = nullptr;
}

void MslAttribute::clear(MslPools* p)
{
    if (p != nullptr) {
        p->free(value);
    }
    else {
        delete value;
    }
    value = nullptr;
}

MslAttributePool::MslAttributePool()
{
    setName("MslObject");
    setObjectSize(sizeof(MslObject));
    fluff();
}

MslAttributePool::~MslAttributePool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
MslPooledObject* MslAttributePool::alloc()
{
    return new MslAttribute();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
MslAttribute* MslAttributePool::newObject()
{
    return (MslAttribute*)checkout();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
