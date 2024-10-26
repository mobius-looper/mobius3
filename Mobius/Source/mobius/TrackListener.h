
#pragma once

class TrackListener
{
  public:

    virtual ~TrackListener() {}

    virtual void trackRecordStart(class TrackProperties& props) = 0;
    virtual void trackRecordStop(class TrackProperties& props) = 0;

};

    
