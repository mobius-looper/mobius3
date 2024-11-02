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
    void analyzeFile();
    void loadFile(int trackNumber, int loopNumber);

    void filesDropped(const juce::StringArray& files, int track, int loop);
    
    void saveFile();
    void saveFile(int trackNumber, int loopNumber);
    void dragOut(int trackNumber, int loopNumber);
    
  private:

    class Supervisor* supervisor;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFolder;
    int destinationTrack = 0;
    int destinationLoop = 0;
    bool analyze = false;
    
    // parse state
    class MidiEvent* heldNotes = nullptr;
    int tsigNumerator = 0;
    int tsigDenominator = 0;
    double secondsPerQuarter = 0.0f;
    double highest = 0.0f;
    
    void chooseMidiFile();
    void doFileLoad(juce::File file);

    void chooseMidiSaveFile();
    void doFileSave(juce::File file);
    
    void analyzeFile(juce::File file);
    void analyzeTrack(int track, int timeFormat, const juce::MidiMessageSequence* seq, juce::String& buffer);
    void analyzeMetaEvent(juce::MidiMessage& msg, int timeFormat, juce::String& buffer);

    class MidiSequence* toSequence(juce::File file);
    double toSequence(const juce::MidiMessageSequence* mms, class MidiSequence* seq,
                      bool merge);
    class MidiEvent* findNoteOn(juce::MidiMessage& msg);
    void finalizeSequence(class MidiSequence* seq, double last);

};
