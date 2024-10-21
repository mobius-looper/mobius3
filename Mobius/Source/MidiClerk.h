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
    void dumpTrack(int i, const juce::MidiMessageSequence* seq);
    
};
