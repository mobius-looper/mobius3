/**
 * A loop is like an onion, it has layers
 */

#pragma once

#include <JuceHeader.h>

class MidiLoop
{
  public:

    MidiLoop(class MidiTrack* t, class MidiLayerPool* p);
    ~MidiLoop();

    void reset();
    void add(class MidiLayer* l);
    int getFrames();

    class MidiLayer* undo();
    class MidiLayer* redo();

    int getLayerCount();
    int getRedoCount();
    MidiLayer* getPlayLayer();
    
    int number = 0;
    
  private:

    class MidiTrack* track = nullptr;
    class MidiLayerPool* layerPool = nullptr;

    // the active layer (head) and undo layers
    class MidiLayer* layers = nullptr;
    int layerCount = 0;
    
    // the redo layers
    class MidiLayer* redoLayers = nullptr;
    int redoCount = 0;

    void reclaimLayers(MidiLayer* list);

};

