/**
 * Utility class to watch for long presses of a function trigger.
 * A more recent adaptation of core/TriggerState
 *
 * Not specifically related to MIDI tracks.
 * This could go up in Kernel, or even Binderator.
 */

#pragma once

class LongWatcher
{
  public:

    /**
     * Class to be notified when a long press is detected.
     */
    class Listener {
      public:
        virtual ~Listener() {}
        virtual void longPressDetected(class UIAction* a) = 0;
    };
    
    LongWatcher();
    ~LongWatcher();
    void initialize(class Session* ses, int sampleRate);
    void setListener(Listener* l);
    
    void watch(class UIAction* a);
    void advance(int blockFrames);

  private:

    class State {
      public:
        // chain pointer for pool and press list
        State* next = nullptr;
        // unique identifier of the trigger that caused the action
        int sustainId = 0;
        // associated function symbol
        class Symbol* symbol = nullptr;
        // track number
        int scope = 0;
        // number of frames held
        int frames = 0;
        // number of times we've been fired
        int notifications = 0;
    };

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
