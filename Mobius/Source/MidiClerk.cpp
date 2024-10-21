
#include <JuceHeader.h>

#include "util/Trace.h"

#include "Supervisor.h"

#include "MidiClerk.h"


MidiClerk::MidiClerk(Supervisor* s)
{
    supervisor = s;
}

MidiClerk::~MidiClerk()
{
}

void MidiClerk::loadFile()
{
    // this does it's thing async then calls back to doFileChosen
    chooseMidiFile();
}

void MidiClerk::chooseMidiFile()
{
    juce::File startPath(supervisor->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;

    juce::String title = "Select a MIDI file...";

    chooser = std::make_unique<juce::FileChooser> (title, startPath, "*.mid");

    auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            // chooserFlags should have only allowed one
            juce::File file = result[0];
            
            doFileLoad(file);

            // remember this directory for the next time
            lastFolder = file.getParentDirectory().getFullPathName();
        }
        
    });
}

void MidiClerk::doFileLoad(juce::File file)
{
    Trace(2, "MidiClerk: Selected file %s", file.getFullPathName().toUTF8());

    juce::FileInputStream fstream(file);
    juce::MidiFile mfile;

    bool status = mfile.readFrom(fstream);
    if (!status)
      Trace(2, "MidiClerk: File could not be read");
    else {
        int ntracks = mfile.getNumTracks();
        Trace(2, "File has %d tracks", ntracks);

        for (int i = 0 ; i < ntracks ; i++) {
            const juce::MidiMessageSequence* seq = mfile.getTrack(i);
            dumpTrack(i, seq);
        }
    }
}

void MidiClerk::dumpTrack(int track, const juce::MidiMessageSequence* seq)
{
    Trace(2, "MidiClerk: Track %d has %d events", track, seq->getNumEvents());

    // not sure how the iterator works...
    // for (auto holder : seq) {

    int max = 20;
    for (int i = 0 ; i < seq->getNumEvents() && i < max ; i++) {
        juce::MidiMessageSequence::MidiEventHolder* holder = seq->getEventPointer(i);
        char buf[32];
        snprintf(buf, sizeof(buf), "%f", (float)holder->message.getTimeStamp());
        Trace(2, "%s: %s", buf, holder->message.getDescription().toUTF8());
    }
}

