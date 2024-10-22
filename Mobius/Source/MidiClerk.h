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

    // parse state
    class MidiEvent* heldNotes = nullptr;
    int tsigNumerator = 0;
    int tsigDenominator = 0;
    double secondsPerQuarter = 0.0f;
    
    void chooseMidiFile();
    void doFileLoad(juce::File file);
    
    void analyzeFile(juce::File file);
    void analyzeTrack(int track, int timeFormat, const juce::MidiMessageSequence* seq, juce::String& buffer);
    void analyzeMetaEvent(juce::MidiMessage& msg, int timeFormat, juce::String& buffer);

    class MidiSequence* toSequence(juce::File file);
    void toSequence(const juce::MidiMessageSequence* mms, class MidiSequence* seq);
    class MidiEvent* findNoteOn(juce::MidiMessage& msg);

};
