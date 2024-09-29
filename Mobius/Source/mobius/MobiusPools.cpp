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
}

void MobiusPools::initialize()
{
}

void MobiusPools::fluff()
{
}

///////////////////////////////////////////////////////////////////////////////
//
// Notifications
//
///////////////////////////////////////////////////////////////////////////////

Notification* MobiusPools::newNotification()
{
    return notificationPool.newNotification();
}

void MobiusPools::checkin(Notification* v)
{
    notificationPool.checkin(v);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/



