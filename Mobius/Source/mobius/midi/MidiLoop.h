/**
 * A loop is like an onion, it has layers
 */

#pragma once

#include <JuceHeader.h>

class MidiLoop
{
  public:

    MidiLoop(class MidiTrack* t);
    ~MidiLoop();

    void initialize();
    void reset();
    void add(class MidiLayer* l);
    
    int number = 0;
    
  private:

    class MidiTrack* track = nullptr;
    class MidiLayerPool* layerPool = nullptr;

    // the active layer (head) and undo layers
    MidiLayer* layers = nullptr;

    // the redo layers
    MidiLayer* redoLayers = nullptr;

    void reclaimLayers(MidiLayer* list);

};

