/**
 * Utility class that watches MIDI events and tracks held notes.
 * Eventually will watch continuous contoller values.
 *
 * Expected to be subclassed by something that implements the three
 * notify methods to insert further processing of the event after tracking.
 *
 */

#pragma once

class MidiWatcher
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void watchedNoteOn(class MidiEvent* e) = 0;
        virtual void watchedNoteOff(class MidiEvent* on, class MidiEvent* off) = 0;
        virtual void watchedEvent(class MidiEvent* e) = 0;
    };

    MidiWatcher();
    ~MidiWatcher();

    void initialize(class MidiEventPool* pool);
    void setListener(Listener* l);

    void midiEvent(class MidiEvent* e);
    void add(class MidiEvent* n);
    void advanceHeld(int blockFrames);
    void flushHeld();
    class MidiEvent* getHeldNotes();

  private:

    class MidiEventPool* midiPool = nullptr;
    class Listener* listener = nullptr;
    class MidiEvent* heldNotes = nullptr;
    
    MidiEvent* removeHeld(class MidiEvent* e);
    
};    
