/**
 * New manager for pooled objects within the Mobius engine.
 * Created for Notifications, gradually migrate older pools to use this too.
 */

#pragma once

#include "Notification.h"

class MobiusPools
{
  public:

    MobiusPools();
    ~MobiusPools();
    
    // fill out the initial set of pooled objects
    // where does configuration come from?
    void initialize();

    // called in the shell maintenance thread to replenish the pools
    // if they dip below their pool threshold
    void fluff();

    class Notification* newNotification();
    void checkin(class Notification* n);

  private:

    NotificationPool notificationPool;
    
};

    
