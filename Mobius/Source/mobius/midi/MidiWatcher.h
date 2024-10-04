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
        virtual void watchedNoteOn(class MidiEvent* e, class MidiNote* n) = 0;
        virtual void watchedNoteOff(class MidiEvent* e, class MidiNote* n) = 0;
        virtual void watchedEvent(class MidiEvent* e) = 0;
    };

    MidiWatcher();
    ~MidiWatcher();

    void initialize(class MidiNotePool* npool);
    void setListener(Listener* l);

    void midiEvent(class MidiEvent* e);
    void add(class MidiNote* n);
    void advanceHeld(int blockFrames);
    void flushHeld();
    class MidiNote* getHeldNotes();

    virtual void notifyNoteOn(class MidiEvent* e, class MidiNote* n) {
        (void)e;
        (void)n;
    }
    virtual void notifyNoteOff(class MidiEvent* e, class MidiNote* n) {
        (void)e;
        (void)n;
    }
    virtual void notifyMidiEvent(class MidiEvent* e) {
        (void)e;
    }

  private:

    class MidiNotePool* notePool = nullptr;
    class Listener* listener = nullptr;
    class MidiNote* heldNotes = nullptr;
    
    MidiNote* removeHeld(class MidiEvent* e);
    
};    
