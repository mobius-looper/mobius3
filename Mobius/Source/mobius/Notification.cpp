
#include <JuceHeader.h>

#include "../model/ObjectPool.h"

#include "Notification.h"

Notification::Notification()
{
}

Notification::~Notification()
{
}

void Notification::poolInit()
{
    next = nullptr;
    id = NotificationNone;
    trackNumber = 0;
    loopFrame = 0;
    mode = nullptr;
}

NotificationPool::NotificationPool()
{
    setName("Notification");
    fluff();
}

NotificationPool::~NotificationPool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
PooledObject* NotificationPool::alloc()
{
    return new Notification();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
Notification* NotificationPool::newNotification()
{
    return (Notification*)checkout();
}
