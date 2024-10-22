/**
 * Emerging support for loading and saving .mid MIDI files
 *
 */

#pragma once

class MidiClerk
{
  public:

    MidiClerk(class Supervisor* super);
    ~MidiClerk();

    void loadFile();
    
  private:

    class Supervisor* supervisor;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFolder;

    void chooseMidiFile();
    void doFileLoad(juce::File file);
    
    void traceFile(juce::File file);
    void traceTrack(int i, int timeFormat, const juce::MidiMessageSequence* seq);
    void traceMetaEvent(juce::MidiMessage& msg, int timeFormat);
    
    void convertFile(juce::File file);
    void convertTrack(int track, int timeFormat, const juce::MidiMessageSequence* seq, juce::String& buffer);
    void convertMetaEvent(juce::MidiMessage& msg, int timeFormat, juce::String& buffer);

    class MidiSequence* toSequence(juce::File file);
    void toSequence(const juce::MidiMessageSequence* mms, class MidiSequence* seq);
    class MidiEvent* findNoteOn(juce::MidiMessage& msg);

    class MidiEvent* heldNotes = nullptr;

};
