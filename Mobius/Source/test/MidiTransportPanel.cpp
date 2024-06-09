/**
 * A testing panel that shows the state of MIDI realtime events
 * and provides basic controls for starting and stopping the internal
 * clock generator.
 *
 * The transport for outgoing MIDI clocks would not normally be used
 * except for testing.   Normally Synchronizer controls this as loops
 * are created and a track becomes the output sync master.
 *
 * Watching the status of incomming messages is however marginally useful
 * for users, so consider factoring this part out into a standard DisplayElement
 * that can be added to the StatusArea.
 */

#include <JuceHeader.h>

#include "../Supervisor.h"
#include "../midi/MidiRealizer.h"
#include "../ui/JuceUtil.h"

#include "BasicInput.h"

MidiTransportPanel::MidiTransportPanel()
{
    addAndMakeVisible(resizer);
    resizer.setBorderThickness(juce::BorderSize<int>(4));
    // keeps the resizer from warping this out of existence
    resizeConstrainer.setMinimumHeight(20);
    resizeConstrainer.setMinimumWidth(20);

    footerButtons.setListener(this);
    footerButtons.setCentered(true);
    footerButtons.add(&closeButton);
    addAndMakeVisible(&footerButtons);
    
    commandButtons.setListener(this);
    commandButtons.setCentered(true);
    commandButtons.add(&startButton);
    commandButtons.add(&stopButton);
    commandButtons.add(&continueButton);
    addAndMakeVisible(commandButtons);

    // sizing on this is way off, there are about 10 chars in the labels but
    // whatever this does is way too wide, cut it in half
    form.setLabelCharWidth(5);
    form.add(&outStatus);
    form.add(&outStarted);
    form.add(&outTempo, this);
    form.add(&outBeat);
    form.add(&inStatus);
    form.add(&inStarted);
    form.add(&inTempo);
    form.add(&inBeat);
    addAndMakeVisible(form);
    
    // todo: let this auto size
    setSize (400, 500);
}

MidiTransportPanel::~MidiTransportPanel()
{
}

void MidiTransportPanel::show()
{
    JuceUtil::center(this);

    // seemed to be having difficulty setting this at construction
    // so get it before showing, why?
    realizer = Supervisor::Instance->getMidiRealizer();
    outTempo.setText(juce::String(realizer->getTempo()));
    
    update();
    
    setVisible(true);
}

void MidiTransportPanel::hide()
{
    setVisible(false);
}

void MidiTransportPanel::start()
{
    realizer->start();
    update();
}

void MidiTransportPanel::stop()
{
    realizer->stop();
    update();
}

void MidiTransportPanel::cont()
{
    realizer->midiContinue();
    update();
}

const int MidiTransportPanelHeaderHeight = 20;

void MidiTransportPanel::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    resizer.setBounds(area);
    
    area.removeFromTop(MidiTransportPanelHeaderHeight);
    
    commandButtons.setBounds(area.removeFromTop(commandButtons.getHeight()));
    footerButtons.setBounds(area.removeFromBottom(MidiTransportPanelHeaderHeight));
    form.setBounds(area);
}

void MidiTransportPanel::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();
    
    // todo: figure out how opaque components work so we dont' have to do this
    g.setColour(juce::Colours::white);
    g.fillRect(area);

    juce::Rectangle<int> header = area.removeFromTop(MidiTransportPanelHeaderHeight);
    g.setColour(juce::Colours::blue);
    g.fillRect(header);
    juce::Font font (MidiTransportPanelHeaderHeight * 0.8f);
    g.setFont(font);
    g.setColour(juce::Colours::white);
    g.drawText("MIDI Transport", header, juce::Justification::centred);
    
    //g.setColour(juce::Colour(MobiusBlue));
    //g.drawRect(getLocalBounds(), 1);
}

void MidiTransportPanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.getMouseDownY() < MidiTransportPanelHeaderHeight) {
        dragger.startDraggingComponent(this, e);
        // the first argu is "minimumWhenOffTheTop" set
        // this to the full height and it won't allow dragging the
        // top out of boundsa
        dragConstrainer.setMinimumOnscreenAmounts(getHeight(), 100, 100, 100);
        dragging = true;
    }
}

void MidiTransportPanel::mouseDrag(const juce::MouseEvent& e)
{
    dragger.dragComponent(this, e, &dragConstrainer);
}

void MidiTransportPanel::mouseUp(const juce::MouseEvent& e)
{
    (void)e;
    dragging = false;
}

void MidiTransportPanel::buttonClicked(juce::Button* b)
{
    if (b == &closeButton) {
        hide();
    }
    else if (b == &startButton) {
        start();
    }
    else if (b == &stopButton) {
        stop();
    }
    else if (b == &continueButton) {
        cont();
    }
}

/**
 * Only one of these so it has to be tempo, if we have
 * more than one, will need to give BasicInput an accessor
 * for the wrapped Label.
 */
void MidiTransportPanel::labelTextChanged(juce::Label* l)
{
    (void)l;

    juce::String value = outTempo.getText();
    float newTempo = value.getFloatValue();
    if (newTempo > 0.0f)
      realizer->setTempo(newTempo);
}

/**
 * Called during Supervisor's advance() in the maintenance thread.
 * Refresh the whole damn thing if anything changes.
 */
void MidiTransportPanel::update()
{
    if (realizer != nullptr) {
        bool newOutStatus = realizer->isSending();
        if (newOutStatus != lastOutStatus) {
            lastOutStatus = newOutStatus;
            if (newOutStatus)
              outStatus.setAndNotify("Sending...");
            else
              outStatus.setAndNotify("");
        }
    
        bool newOutStarted = realizer->isStarted();
        if (newOutStarted != lastOutStarted) {
            lastOutStarted = newOutStarted;
            if (newOutStarted)
              outStarted.setAndNotify("Started");
            else
              outStarted.setAndNotify("");
        }

        // beats will increment if the clock is left running
        // after Stop, don't watch those
        int newOutBeat = -1;
        if (newOutStarted)
          newOutBeat = realizer->getRawBeat();
    
        if (newOutBeat != lastOutBeat) {
            lastOutBeat = newOutBeat;
            outBeat.setAndNotify(formatBeat(newOutBeat));
        }

        bool newInStatus = realizer->isInputReceiving();
        if (newInStatus != lastInStatus) {
            lastInStatus = newInStatus;
            if (newInStatus)
              inStatus.setAndNotify("Receiving...");
            else
              inStatus.setAndNotify("");
        }

        bool newInStarted = realizer->isInputStarted();
        if (newInStarted != lastInStarted) {
            lastInStarted = newInStarted;
            if (newInStarted)
              inStarted.setAndNotify("Started");
            else
              inStarted.setAndNotify("");
        }
    
        int newInTempo = realizer->getInputSmoothTempo();
        if (newInTempo != lastInTempo) {
            lastInTempo = newInTempo;
            inTempo.setAndNotify(formatTempo(newInTempo));
        }

        int newInBeat = -1;
        if (newInStarted)
          newInBeat = realizer->getInputRawBeat();
    
        if (newInBeat != lastInBeat) {
            lastInBeat = newInBeat;
            inBeat.setAndNotify(formatBeat(newInBeat));
        }
    }
}

/**
 * For reference, this is the usual pattern for forcing repaint if
 * we were drawing text rather than using components. Keeping this
 * around temporarily as a reminder to build something general
 * that does this.
 */
#if 0
void MidiTransportPanel::update()
{
    // this little dance we do all the time, would be nice
    // to have some sort of utility container to automate this
    bool outStatus = realizer->isSending();
    bool outStarted = realizer->isStarted();
    int outBeat = realizer->getRawBeat();
    bool inStatus = realizer->isInputReceiving();
    bool inStarted = realizer->isInputStarted();
    int inTempo = realizer->getSmoothTempo();
    int inBeat = realizer->getInputBeat();
    
    if (outStatus != lastOutStatus ||
        outStarted != lastOutStarted ||
        outBeat != lastOutBeat ||
        inStatus != lastInStatus ||
        inStarted != lastInStarted ||
        inTempo != lastInTempo ||
        inBeat != lastInBeat) {

        lastOutStatus = outStatus;
        lastOutStarted = outStarted;
        lastOutBeat = outBeat;
        lastInStatus = inStatus;
        lastInStarted = inStarted;
        lastInTempo = inTempo;
        lastInBeat = inBeat;

        updateAll();
    }
}
#endif

juce::String MidiTransportPanel::formatBeat(int rawbeat)
{
    juce::String beatBar;
    
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
    
    return beatBar;
}

juce::String MidiTransportPanel::formatTempo(int tempo)
{
    int main = tempo / 10;
    int fraction = tempo % 10;
    return juce::String(main) + "." + juce::String(fraction);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
