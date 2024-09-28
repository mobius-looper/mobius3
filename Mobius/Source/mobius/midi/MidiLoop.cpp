
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
        list->clear();
        list->next = nullptr;
        layerPool->checkin(list);
        list = next;
    }
}

void MidiLoop::add(MidiLayer* l)
{
    l->next = layers;
    layers = l;
}
