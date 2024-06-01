/**
 * Class managing most configuration editing dialogs.
 * Old Mobius implemented these with popup windows, we're now doing
 * these with simple Juce components overlayed over the main window.
 *
 * There are a number of panels focused in a particular area of the
 * configuration: global, presets, setups, bindings.  Only one of these
 * may be visible at a time.
 * 
 * These methods may be used to show editors for various parts of the
 * configuration:
 *
 *   showGlobal
 *   showPresets
 *   showSetups
 *   showMIDIBindings
 *   showKeyboardBindings
 *   showPluginParameters
 *   showScripts
 *   showSamples
 *
 * Although they are stored in different files, might as well do the UI configuration
 * here too:
 *
 *   showButtons
 *   showDisplayComponents
 *
 * Only one configuration editor may be open at a time, if a request is made to show one
 * that is not already visible it will be hidden, but the editing session will remain.
 * The editing session is only closed when the user explicitly clicks one of the close buttons
 * or when forced to cancel by something else.
 *
 * The closeAll() method may be used to close all active configuration editors without having
 * to manually click the save or cancel buttons.
 *
 * Note that this is NOT a juce::Component.  It is responsible for constructing the appropriate
 * Components and managing their visibility and will clean up allocations when it is deleted.
 */

#pragma once

#include <JuceHeader.h>

#include "ConfigPanel.h"
#include "GlobalPanel.h"
#include "PresetPanel.h"
#include "SetupPanel.h"
#include "ButtonPanel.h"
#include "KeyboardPanel.h"
#include "MidiPanel.h"
#include "MidiDevicesPanel.h"
#include "AudioDevicesPanel.h"
#include "ScriptPanel.h"
#include "SamplePanel.h"
#include "DisplayPanel.h"
#include "HostPanel.h"

class ConfigEditor
{
  public:

    ConfigEditor(juce::Component* argOwner);
    ~ConfigEditor();

    // sigh, would like to pass this to the constructor
    // but have to work out how the member initialization works
    // in MainWindow
    void init(class Supervisor* supervisor);

    void showGlobal();
    void showPresets();
    void showSetups();
    void showMidiBindings();
    void showKeyboardBindings();
    void showPluginParameters();
    void showScripts();
    void showSamples();
    void showButtons();
    void showMidiDevices();
    void showAudioDevices();
    void showDisplay();
    void showHostParameters();
    
    void closeAll();

    // should be protected with friends for the panels
    void close(ConfigPanel* p);
    class MobiusConfig* getMobiusConfig();
    void saveMobiusConfig();
    class UIConfig* getUIConfig();
    void saveUIConfig();
    
  private:

    void addPanel(ConfigPanel* panel);
    void show(ConfigPanel* panel);
    
    class Supervisor* supervisor = nullptr;
    juce::Component* owner = nullptr;

    bool initialized = false;

    // todo: these are unused, get rid of them
    //class MobiusConfig* masterConfig = nullptr;
    //class UIConfig* masterUIConfig = nullptr;
    
    GlobalPanel global {this};
    PresetPanel presets {this};
    SetupPanel setups {this};
    ButtonPanel buttons {this};
    KeyboardPanel keyboard {this};
    MidiPanel midi {this};
    MidiDevicesPanel midiDevices {this};
    AudioDevicesPanel audioDevices {this};
    ScriptPanel scripts {this};
    SamplePanel samples {this};
    DisplayPanel display {this};
    HostPanel hostParameters {this};
    
    // list of all active panels, we often need to iterate over these
    juce::Array<ConfigPanel*> panels;
};

