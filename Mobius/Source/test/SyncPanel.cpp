/**
 * A testing panel that shows the state of synchronization within the engine.
 * 
 */

#include <JuceHeader.h>

#include "../Supervisor.h"
#include "../util/Trace.h"
#include "../model/MobiusState.h"
#include "../midi/MidiRealizer.h"
#include "../mobius/MobiusInterface.h"
#include "../ui/JuceUtil.h"

#include "BasicInput.h"
#include "BasicLog.h"

#include "SyncPanel.h"

SyncContent::SyncContent()
{
    commandButtons.setListener(this);
    commandButtons.setCentered(true);
    //commandButtons.add(&startButton);
    //commandButtons.add(&stopButton);
    //commandButtons.add(&continueButton);
    //addAndMakeVisible(commandButtons);

    // sizing on this is way off, there are about 10 chars in the labels but
    // whatever this does is way too wide, cut it in half
    form.setLabelCharWidth(5);
    form.add(&hostStatus);
    form.add(&hostTempo);
    form.add(&hostBeat);
    addAndMakeVisible(form);

    addAndMakeVisible(&log);
}

SyncContent::~SyncContent()
{
    GlobalTraceListener = nullptr;
}

void SyncContent::showing()
{
    update();
    
    // start intercepting trace messages
    // TestPanel also does this so if you try to open them both at the same
    // time, there will be a battle over who gets it
    GlobalTraceListener = this;
}

void SyncContent::hiding()
{
    GlobalTraceListener = nullptr;
}

void SyncContent::start()
{
    //realizer->start();
    update();
}

void SyncContent::stop()
{
    //realizer->stop();
    update();
}

void SyncContent::cont()
{
    //realizer->midiContinue();
    update();
}

const int SyncContentHeaderHeight = 20;

void SyncContent::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    commandButtons.setBounds(area.removeFromTop(commandButtons.getHeight()));
    form.setBounds(area.removeFromTop(form.getHeight()));

    log.setBounds(area);
}

void SyncContent::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();
    
    // todo: figure out how opaque components work so we dont' have to do this
    g.setColour(juce::Colours::white);
    g.fillRect(area);
}

void SyncContent::buttonClicked(juce::Button* b)
{
    if (b == &startButton) {
        start();
    }
    else if (b == &stopButton) {
        stop();
    }
    else if (b == &continueButton) {
        cont();
    }
    else {
        // shouldn't need this
        //BasePanel::buttonClicked(b);
    }
}

/**
 * Only one of these so it has to be tempo, if we have
 * more than one, will need to give BasicInput an accessor
 * for the wrapped Label.
 */
void SyncContent::labelTextChanged(juce::Label* l)
{
    (void)l;
}

/**
 * Called during Supervisor's advance() in the maintenance thread.
 * Refresh the whole damn thing if anything changes.
 */
void SyncContent::update()
{
    MobiusInterface* mobius = Supervisor::Instance->getMobius();
    MobiusState* state = mobius->getState();
    MobiusSyncState* sync = &(state->sync);

    // same pattern repeats three times, factor something that can be reused
    if (!sync->inStarted) {
        if (inStatus.getText() != "") inStatus.setAndNotify("");
        if (inTempo.getText() != "") inTempo.setAndNotify("");
        if (inBeat.getText() != "") inBeat.setAndNotify("");
    }
    else {
        if (inStatus.getText() == "") inStatus.setAndNotify("Receiving");
        inTempo.setAndNotify(formatTempo(sync->inTempo));
        inBeat.setAndNotify(formatBeat(sync->inBeat));
    }
    
    if (!sync->outStarted) {
        if (outStatus.getText() != "") outStatus.setAndNotify("");
        if (outTempo.getText() != "") outTempo.setAndNotify("");
        if (outBeat.getText() != "") outBeat.setAndNotify("");
    }
    else {
        if (outStatus.getText() == "") outStatus.setAndNotify("Sending");
        outTempo.setAndNotify(formatTempo(sync->outTempo));
        outBeat.setAndNotify(formatBeat(sync->outBeat));
    }

    if (!sync->hostStarted) {
        if (hostStatus.getText() != "") hostStatus.setAndNotify("");
        if (hostTempo.getText() != "") hostTempo.setAndNotify("");
        if (hostBeat.getText() != "") hostBeat.setAndNotify("");
    }
    else {
        if (hostStatus.getText() == "") hostStatus.setAndNotify("Receiving");
        hostTempo.setAndNotify(formatTempo(sync->hostTempo));
        hostBeat.setAndNotify(formatBeat(sync->hostBeat));
    }
}

juce::String SyncContent::formatBeat(int rawbeat)
{
    juce::String beatBar;

    bool doBars = false;

    if (doBars) {
        // negative means the transport is in a stopped state
        // but clocks may still be running
        if (rawbeat >= 0) {
            // don't have state for this yet, assume 4
            int beatsPerBar = 4;
            int beat = rawbeat % beatsPerBar;
            int bar = rawbeat / beatsPerBar;

            //if (beat > 0 || bar > 0)
            beatBar += juce::String(bar) + "/" + juce::String(beat);
        }
    }
    else {
        beatBar = juce::String(rawbeat);
    }
    
    return beatBar;
}

juce::String SyncContent::formatTempo(float tempo)
{
    // this was for MidiTransport which held tempos as a int x100
    //int main = tempo / 10;
    //int fraction = tempo % 10;
    //return juce::String(main) + "." + juce::String(fraction);
    return juce::String(tempo);
}

/**
 * Intercepts Trace log flushes and puts them in the sync log.
 */
void SyncContent::traceEmit(const char* msg)
{
    juce::String jmsg(msg);
    
    // these will usually have a newline already so don't add another
    // one which log() will do
    log.add(jmsg);

    // look for a few keywords for interesting messages
#if 0
    if (jmsg.contains("ERROR") ||
        jmsg.startsWith("TestStart") ||
        jmsg.contains("Warp") ||
        jmsg.contains("Alert"))
      summary.add(jmsg);
#endif    
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
