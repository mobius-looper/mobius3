
#include <JuceHeader.h>

#include "MidiTracker.h"
#include "MidiTrack.h"
#include "MidiLoop.h"

MidiLoop::MidiLoop(MidiLayerPool* pool)
{
    layerPool = pool;
}

MidiLoop::~MidiLoop()
{
}

void MidiLoop::reset()
{
    reclaimLayers(layers);
    layers = nullptr;
    layerCount = 0;
    reclaimLayers(redoLayers);
    redoLayers = nullptr;
    redoCount = 0;
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
    layerCount++;
}

int MidiLoop::getLayerCount()
{
    return layerCount;
}

int MidiLoop::getRedoCount()
{
    return redoCount;
}

int MidiLoop::getFrames()
{
    int frames = 0;
    if (layers != nullptr)
      frames = layers->getFrames();
    return frames;
}

MidiLayer* MidiLoop::undo()
{
    if (layers == nullptr) {
        // empty loop
    }
    else if (layers->next == nullptr) {
        // only one layer, can't go back to nothing
    }
    else {
        MidiLayer* undone = layers;
        layers = layers->next;
        layerCount--;

        undone->next = redoLayers;
        redoLayers = undone;
        redoCount++;

        // todo: configurable redo limit
        int maxRedo = 4;
        if (redoCount >= maxRedo) {
            // don't trust the count
            MidiLayer* last = redoLayers;
            for (int i = 1 ; i < maxRedo && last != nullptr ; i++)
              last = last->next;

            if (last == nullptr) {
                // count was higher than the number of layers, could fix it I guess
                Trace(1, "MidiLoop: Redo count messed up");
            }
            else {
                MidiLayer* garbage = last->next;
                last->next = nullptr;
                reclaimLayers(garbage);
            }
        }
    }
    return layers;
}

MidiLayer* MidiLoop::redo()
{
    if (redoLayers == nullptr) {
        // no where to go
    }
    else {
        MidiLayer* redone = redoLayers;
        redoLayers = redoLayers->next;
        redoCount--;
        
        redone->next = layers;
        layers = redone;
        layerCount++;
    }
    return layers;
}
    
MidiLayer* MidiLoop::getPlayLayer()
{
    return layers;
}
