
#include <JuceHeader.h>

#include "../model/ObjectPool.h"

#include "MslPools.h"
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
    attributes = nullptr;
}

void MslObject::clear(MslPools* p)
{
    while (attributes != nullptr) {
        MslAttribute* next = attributes->next;
        attributes->next = nullptr;
        if (p != nullptr) {
            attributes->clear(p);
            p->free(attributes);
        }
        else {
            delete attributes;
        }
        attribute = next;
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
    value.setNull();
    next = nullptr;
}

void MslAttribute::clear(MslPools* p)
{
    if (p != nullptr) {
        p->clear(&value);
    }
    else {
        // todo: not sure I want't to mess with deletion of these?
        // should be passing a pool
    }
    value.setNull();
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
