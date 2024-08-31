/**
 * Note that access to this can't escape the kernel so we don't need a csect
 */

#include "../util/Trace.h"

#include "Notification.h"
#include "MobiusPools.h"

MobiusPools::MobiusPools()
{
}

MobiusPools::~MobiusPools()
{
    Trace(2, "MobiusPools: destructing");
    flushNotifications();

    traceStatistics();
}

void MobiusPools::initialize()
{
}

void MobiusPools::fluff()
{
}

void MobiusPools::traceStatistics()
{
    Trace(2, "MobiusPools: created/requested/returned/deleted/pooled");

    // sigh, it sure would be nice if these had a common superclass for the chain pointer
    int count = 0;
    for (Notification* obj = notificationPool ; obj != nullptr ; obj = obj->next) count++;
    Trace(2, "  notifications: %d %d %d %d",
          notificationsCreated, notificationsRequested, notificationsReturned, notificationsDeleted);

}

///////////////////////////////////////////////////////////////////////////////
//
// Notifications
//
///////////////////////////////////////////////////////////////////////////////

void MobiusPools::flushNotifications()
{
    while (notificationPool != nullptr) {
        Notification* next = notificationPool->next;
        notificationPool->next = nullptr;
        delete notificationPool;
        notificationPool = next;
        notificationsDeleted++;
    }
}

Notification* MobiusPools::allocNotification()
{
    Notification* v = nullptr;

    if (notificationPool != nullptr) {
        v = notificationPool;
        notificationPool = notificationPool->next;
        v->next = nullptr;
        v->init();
    }
    else {
        // complain loudly that the fluffer isn't doing it's job
        v = new Notification();
        notificationsCreated++;
    }
    notificationsRequested++;
    return v;
}

void MobiusPools::free(Notification* v)
{
    if (v != nullptr) {
        v->next = notificationPool;
        notificationPool = v;
        notificationsReturned++;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/



