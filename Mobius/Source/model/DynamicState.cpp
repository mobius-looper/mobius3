
#include "ObjectPool.h"
#include "DynamicState.h"

void DynamicState::startWrite()
{
    events.startWrite();
    regions.startWrite();
    layers.startWrite();
}

DynamicEvent* DynamicState::nextWriteEvent()
{
    return events.nextWrite();
}

DynamicRegion* DynamicState::nextWriteRegion()
{
    return regions.nextWrite();
}

DynamicLayer* DynamicState::nextWriteLayer()
{
    return layers.nextWrite();
}

void DynamicState::commitWrite()
{
    events.commitWrite();
    regions.commitWrite();
    layers.commitWrite();
}

DynamicEvent* DynamicState::nextReadEvent()
{
    return events.nextRead();
}

DynamicRegion* DynamicState::nextReadRegion()
{
    return regions.nextRead();
}

DynamicLayer* DynamicState::nextReadLayer()
{
    return layers.nextRead();
}
