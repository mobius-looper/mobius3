/**
 * Utility class that gathers most of the calculations surrounding
 * "unit lengths".
 *
 * These are closely related and evolving and I like to see them all in
 * one place rather than strewn about between SyncMaster and BarTender.
 *
 */

#pragma once

#include <JuceHeader.h>

class Unitarian
{
  public:

    Unitarian(class SyncMaster* sm);
    ~Unitarian();

    int getUnitLength(SyncSource src);
    int getTrackUnitLength(class LogicalTrack* t, TrackSyncUnit unit);
    int getTrackUnitLength2(class LogicalTrack* t, TrackSyncUnit unit);
    int getLeaderUnitLength(class LogicalTrack* follower, TrackSyncUnit unit);

    int getSingleAutoRecordUnitLength(class LogicalTrack* track);
    int getLockUnitLength(class LogicalTrack* track);

    void verifySyncLength(LogicalTrack* lt);
    int getActiveFollowers(SyncSource src, int unitLength);
    
 private:

    class SyncMaster* syncMaster;
    class BarTender* barTender;
    
    int calculateUnitLength(class LogicalTrack* lt);
    int getActiveFollowers(SyncSource src, int unitLength, class LogicalTrack* notThisOne);

};
 
