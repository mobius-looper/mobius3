
#include <JuceHeader.h>

#include "../Supervisor.h"
#include "../MidiManager.h"

#include "JuceUtil.h"
#include "MidiMonitorPanel.h"

//////////////////////////////////////////////////////////////////////
//
// MidiMonitorPanel
//
//////////////////////////////////////////////////////////////////////

MidiMonitorPanel::MidiMonitorPanel()
{
    setTitle("MIDI Monitor");
    
    setContent(&content);
    addButton(&clearButton);
    
    setSize(600, 600);
}

MidiMonitorPanel::~MidiMonitorPanel()
{
}

void MidiMonitorPanel::showing()
{
    MidiManager* mm = Supervisor::Instance->getMidiManager();
    mm->addMonitor(this);
    
    // let the log say hello
    content.showing();
}

void MidiMonitorPanel::hiding()
{
    MidiManager* mm = Supervisor::Instance->getMidiManager();
    mm->removeMonitor(this);
}

void MidiMonitorPanel::footerButton(juce::Button* b)
{
    if (b == &clearButton)
      content.log.clear();
}

/**
 * MidiManager::Monitor
 */

void MidiMonitorPanel::midiMonitor(const juce::MidiMessage& message, juce::String& source)
{
    content.log.midiMessage(message, source);
}

/**
 * MidiPanel and MidiDevicesPanel both want to bypass Binderator so actions
 * don't fire off while you're configuring things.
 *
 * Just simple monitoring doesn't need that, so return false here.
 * todo: add a checkbox to turn bypass on and off.
 */
bool MidiMonitorPanel::midiMonitorExclusive()
{
    return false;
}

//////////////////////////////////////////////////////////////////////
//
// MidiMonitorContent
//
//////////////////////////////////////////////////////////////////////

MidiMonitorContent::MidiMonitorContent()
{
    addAndMakeVisible(log);
}

MidiMonitorContent::~MidiMonitorContent()
{
}

void MidiMonitorContent::showing()
{
    // show the open devices
    // todo: do this every time we're shown or only the first time?
    log.showOpen();
}

void MidiMonitorContent::resized()
{
    log.setBounds(getLocalBounds());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
