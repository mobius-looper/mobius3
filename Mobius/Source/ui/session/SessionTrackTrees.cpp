
#include <JuceHeader.h>

#include "../../Provider.h"

#include "SessionEditorTree.h"
#include "SessionTrackTrees.h"

SessionTrackTrees::SessionTrackTrees()
{
    addAndMakeVisible(audioTree);
    addChildComponent(midiTree);
    showingMidi = false;
}

SessionTrackTrees::~SessionTrackTrees()
{
}

void SessionTrackTrees::load(Provider* p)
{
    audioTree.load(p, juce::String("sessionAudioTrack"));
    midiTree.load(p, juce::String("sessionMidiTrack"));
}

void SessionTrackTrees::showMidi(bool b)
{
    if (b != showingMidi) {
        if (showingMidi) {
            midiTree.setVisible(false);
            audioTree.setVisible(true);
        }
        else {
            audioTree.setVisible(false);
            midiTree.setVisible(true);
        }
        showingMidi = b;
    }
}

void SessionTrackTrees::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    audioTree.setBounds(area);
    midiTree.setBounds(area);
}
