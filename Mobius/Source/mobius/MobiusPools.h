/**
 * New manager for pooled objects within the Mobius engine.
 * Created for Notifications, gradually migrate older pools to use this too.
 */

#pragma once

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

    class Notification* allocNotification();
    void free(class Notification* n);

    void traceStatistics();

  private:
    
    class Notification* notificationPool = nullptr;

    int notificationsCreated = 0;
    int notificationsRequested = 0;
    int notificationsReturned = 0;
    int notificationsDeleted = 0;
    
    void flushNotifications();
    
};

    
