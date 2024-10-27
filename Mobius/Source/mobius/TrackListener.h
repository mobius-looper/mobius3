
#pragma once

#include "Notification.h"

class TrackListener
{
  public:

    virtual ~TrackListener() {}

    virtual void trackNotification(NotificationId id, class TrackProperties& props) = 0;

};

    
