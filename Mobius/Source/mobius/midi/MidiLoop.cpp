
#include <JuceHeader.h>

#include "MidiTracker.h"
#include "MidiTrack.h"
#include "MidiLoop.h"

MidiLoop::MidiLoop(MidiTrack* argTrack)
{
    track = argTrack;
}

MidiLoop::~MidiLoop()
{
}

void MidiLoop::initialize()
{
    MidiTracker* tracker = track->getMidiTracker();
    eventPool = tracker->getMidiPool();
    sequencePool = tracker->getSequencePool();
    layerPool = tracker->getLayerPool();
}

void MidiLoop::reset()
{
    reclaimLayers(layers);
    layers = nullptr;
    reclaimLayers(redoLayers);
    redoLayers = nullptr;
}

void MidiLoop::reclaimLayers(MidiLayer* list)
{
    while (list != nullptr) {
        MidiLayer* next = list->next;
        list->clear(sequencePool, eventPool);
        list->next = nullptr;
        layerPool->checkin(list);
        list = next;
    }
}

void MidiLoop::setInitialLayer(MidiLayer* l)
{
    // I think I want this to be reset first, if not something is wrong
    if (layers != nullptr || redoLayers != nullptr) {
        Trace(1, "MidiLoop: Setting initial layer without clearing me first");
        reset();
    }
    layuers = l;
}
