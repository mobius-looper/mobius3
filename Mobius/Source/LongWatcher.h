/**
 * Utility class to watch for long presses of a function trigger.
 * A more recent adaptation of core/TriggerState
 *
 * Currently maintained by TrackManager but should be moved to Binderator.
 */

#pragma once

#include "model/UIAction.h"

class LongWatcher
{
  public:

    class State {
      public:
        // chain pointer for pool and press list
        State* next = nullptr;
        // unique identifier of the trigger that caused the action
        int sustainId = 0;
        // associated function symbol
        class Symbol* symbol = nullptr;
        // number of frames held
        int frames = 0;
        // number of times we've been fired
        int notifications = 0;

        int value = 0;
        char scope[UIActionScopeMax];
        char arguments[UIActionArgMax];

        // other things we may want to save
        // noQuantize, noSynchronization, noGroup
    };
    
    /**
     * Class to be notified when a long press is detected.
     */
    class Listener {
      public:
        virtual ~Listener() {}
        virtual void longPressDetected(State* s) = 0;
    };
    
    LongWatcher();
    ~LongWatcher();
    void initialize(class Session* ses, int sampleRate);
    void setListener(Listener* l);
    
    void watch(class UIAction* a);
    void advance(int blockFrames);

  private:


    Listener* listener = nullptr;
    int sampleRate = 44100;
    int threshold = 0;
    
    // an object pool of sorts, there can't be many of these
    State* pool = nullptr;
    const int MaxPool = 4;

    // the currently held triggers
    State* presses = nullptr;

    void reclaim(State* list);
};
