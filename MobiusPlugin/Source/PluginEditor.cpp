/**
 * This file was generated by Projucer.
 *
 * Some things we could overload but wasn't in the generated code:
 *
 * getControlHighlight
 * getControlParameterIndex
 * supportsHostMIDIControllerPresence
 * hostMIDIControllerIsAvailable
 *
 * isResizable
 *
 * getHostContext
 *   something to do with host provided context menus
 *   not supported by all hosts
 *
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "util/Trace.h"
#include "Supervisor.h"

//==============================================================================

/**
 * This appears to be a transient object that will be deleted when
 * you close the window.  Supervisor keeps the root UI component alive
 * all the time, so we just attach and deattach from it.  This also
 * serves as the hook within Supervisor to know if the editor is active. 
 */
MobiusPluginAudioProcessorEditor::MobiusPluginAudioProcessorEditor (MobiusPluginAudioProcessor& p, Supervisor* s)
    : AudioProcessorEditor (&p), audioProcessor (p), supervisor(s)
{
    Trace(2, "MobiusPluginAudioProcessorEditor: Constructing");

    rootComponent = supervisor->getPluginEditorComponent();

    addAndMakeVisible(rootComponent);
    
    // first arg is allowHostToResize, second is useBottomRightCornerResizer
    // first worked in Live/Windows, not sure why the second would be necessary
    // there is also setResizeLimits that can set min/max sizes
    setResizable(true, false);

    // let the component determine the size of the window
    // did it already do this?
    // default it if it didn't, this is what MainComponent does
    int width = rootComponent->getWidth();
    int height = rootComponent->getHeight();
    
    if (width == 0) width = 1200;
    if (height == 0) height = 800;

    
    setSize (width, height);
}

MobiusPluginAudioProcessorEditor::~MobiusPluginAudioProcessorEditor()
{
    Trace(2, "MobiusPluginAudioProcessorEditor: Destructing");
    supervisor->closePluginEditor();
}

//==============================================================================

void MobiusPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    //g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // match what MainComponent does in standalone
    // start with basic black, always in style
    g.fillAll (juce::Colours::black);

    /** Projecer example
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
    */
}

void MobiusPluginAudioProcessorEditor::resized()
{
    rootComponent->setBounds(getLocalBounds());
}
