/**
 * This only exists to hide the need for LogicalTrack to get the
 * track number.
 */

#include "LogicalTrack.h"
#include "BaseTrack.h"

int BaseTrack::getNumber()
{
    return logicalTrack->getNumber();
}
