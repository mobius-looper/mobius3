#pragma once

#include <JuceHeader.h>

#include "../../script/MslValue.h"

/**
 * Helper struct to represent occlusions
 */
class SessionOcclusions
{
  public:

    class Occlusion {
      public:
        juce::String source;
        MslValue value;
    };

    Occlusion* get(juce::String key) {
        return map[key];
    }
    
    void add(juce::String source, juce::String key, MslValue* value) {
        Occlusion* o = map[key];
        if (o == nullptr) {
            o = new Occlusion();
            occlusions.add(o);
            map.set(key, o);
        }
        o->source = source;
        o->value.copy(value);
    }

    void clear() {
        map.clear();
        occlusions.clear();
    }
    
  private:
    
    juce::OwnedArray<Occlusion> occlusions;
    juce::HashMap<juce::String,Occlusion*> map;
    
};

