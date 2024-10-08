/**
 * This is the root component for standalone audio applications.
 *
 * It was generated by Projucer with Mobius extensions added.
 * I think we're allowed to put things in here without them being
 * overwritten after changing project settings in Projucer, but it is unclear.
 *
 * Everything complicated was pushed down into Supervisor so it can be shared
 * by both an AudioAppComponent for standalone and an AudioProcessor for plugins.
 *
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "MainComponent.h"

#include "Supervisor.h"

MainComponent::MainComponent() :
    AudioAppComponent(customAudioDeviceManager)
{
    // Jeff's component tree debugging hack
    setName("MainComponent");

    // Startup can do a lot of thigns, perhais we should have different
    // phases, first to load any configuration related to the initial window size
    // and device configuration, and then another to start up the engine.
    // Will want that for VST plugin probing too.
    
    // having some ugly initialization timing issues trying to do too much
    // in the MainComponent constructor, is there a convenient hook to defer that?
    supervisor.start();

    // Start with a size large enough to give us room but still display
    // on most monitors.
    // Supervisor.start will normally have set this from uioconfig if one was saved
    int width = getWidth();
    int height = getHeight();
    if (width == 0) width = 1200;
    if (height == 0) height = 800;
    setSize (width, height);
}

MainComponent::~MainComponent()
{
    Trace(2, "MainComponent: Destructing\n");

    // Projucer: This shuts down the audio device and clears the audio source.
    // Docs:
    // Shuts down the audio device and clears the audio source.
    // This method should be called in the destructor of the derived class
    // otherwise an assertion will be triggered.
    shutdownAudio();

    // this must be done AFTER shutdownAudio so we don't delete things
    // out from under active audio threads
    supervisor.shutdown();
}

/**
 * Focus is complicated, something having to do with action buttons
 * cause MainComponent to lose it?
 * Get here whenever the mouse is clicked within the MobiusDisplay which
 * is all of it except for the menu bar.
 * Weirdly this does not seem to prevent KeyPress events from being
 * sent here because KeyTracker continues working.  Maybe because no subcomponents
 * want focus so it just ends up back here?
 */
void MainComponent::focusLost(juce::Component::FocusChangeType cause)
{
    (void)cause;
    
#if 0
    switch (cause) {
        case juce::Component::FocusChangeType::focusChangedByMouseClick:
            Trace(1, "MainComponent focus lost by mouse click\n");
            break;
        case juce::Component::FocusChangeType::focusChangedByTabKey:
            Trace(1, "MainComponent focus lost by tab key\n");
            break;
        case juce::Component::FocusChangeType::focusChangedDirectly:
            Trace(1, "MainComponent focus lost directly\n");
            break;
    }
#endif    
}

//////////////////////////////////////////////////////////////////////
//
// AudioAppComponent
//
//////////////////////////////////////////////////////////////////////

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    supervisor.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    // bufferToFill.clearActiveBufferRegion();
    supervisor.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    supervisor.releaseResources();
}

//////////////////////////////////////////////////////////////////////
//
// Component
//
//////////////////////////////////////////////////////////////////////

void MainComponent::paint (juce::Graphics& g)
{
    // when the application first starts it does not have focus for some reason
    // this seems to work to get it started
    // can't do this in the constructor because you get an assertion violation because
    // MainComponent isn't visible yet, I'm assuming by the time we hit paint() it's okay
    // to ask for focus.  Unclear what the side effects of this are because it's going to get
    // called on every paint.  Since I'm not doing anything fancy with child keyboard focus
    // and always want it to start here and go through KeyTracker, I don't think it hurts,
    // but focus is extremely complex so who knows.
    // Unclear what this does for plugin editor windows.  I imagine this won't grab
    // focus away from other windows, so the outer window at least must have OS keyboard
    // focus for this to do anything?

    // welp, can't do this because it immediately grabs focus away from any input
    // text fields that are displayed in config panels of the TestPanel, need
    // to find a different way for this just to get focus on startup
    //grabKeyboardFocus();
    
    // start with basic black, always in style
    g.fillAll (juce::Colours::black);
}

void MainComponent::resized()
{
    // This does not cascade through the children automatically
    // unless you call setSize on them, it's unusual here because of the
    // deferred adding of children by DisplayManager, so assume we have
    // something and let it fill us up with hope and wonder
    juce::Component* child = getChildComponent(0);
    if (child != nullptr)
      child->setBounds(getLocalBounds());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
