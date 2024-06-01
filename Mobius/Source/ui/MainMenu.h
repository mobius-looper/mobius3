/**
 * Mobius main menu.
 * 
 * A wrapper around Juce MenuBarComponent that tries to simplify some things.
 * Owners should implmenet our Listener interface which just forwards
 * things from MenuBarModel.
 */

#pragma once

#include <JuceHeader.h>

class MainMenu : public juce::Component, public juce::MenuBarModel
{
  public:

    /**
     * Interface of the thing that wants to receive menu events
     */
    class Listener {
      public:
        virtual ~Listener() {}
        virtual void mainMenuSelection(int id) = 0;
    };

    // these are indexes not ids so must start from zero
    enum MenuIndexes {
        menuIndexFile = 0,
        menuIndexSetup,
        menuIndexPreset,
        menuIndexDisplay,
        menuIndexConfig,
        menuIndexHelp,
        menuIndexTest
    };

    /**
     * Names of the top-level menu items
     */
    juce::StringArray MenuNames {"File", "Track Setups", "Presets", "Display", "Configuration", "Help", "Test"};

    /**
     * Offset of menu item ids for the generated track setup items
     * Figure out a better way to do this so we don't overflow
     */
    static const int MenuSetupOffset = 100;
    static const int MenuSetupMax = 199;
    
    /**
     * Offset of menu item ids for the generated preset items
     */
    static const int MenuPresetOffset = 200;
    static const int MenuPresetMax = 299;

    /**
     * Offsets for extensible items in the Display menu.
     */
    static const int MenuLayoutOffset = 300;
    static const int MenuLayoutMax = 399;
    static const int MenuButtonsOffset = 400;
    static const int MenuButtonsMax = 499;

    /**
     * Display options
     */
    static const int MenuOptionsBorders = 500;
    static const int MenuOptionsIdentify = 501;
    
        
    /**
     * These are menu item model ids which must begin from 1
     */
    enum MenuItems {
        
        // File
        OpenLoop = 1,
        OpenProject,
        SaveLoop,
        SaveProject,
        QuickSave,
        LoadScripts,
        LoadSamples,
        Exit,

        // Configuration
        GlobalParameters,
        Presets,
        TrackSetups,
        MidiControl,
        KeyboardControl,
        Buttons,
        HostParameters,
        DisplayComponents,
        Scripts,
        Samples,
        MidiDevices,
        AudioDevices,

        // Help
        KeyBindings,
        MidiBindings,
        About,

        // test
        TestMode,
        MidiTransport,
        SymbolTable,
        DiagnosticWindow,
        UpgradeConfig,
        
        };
    
    MainMenu();
    ~MainMenu() override;

    void resized() override;

    void setListener(Listener* l) {
        listener = l;
    }

    int getPreferredHeight();

    // show a standalone PopupMenu that has the same structure as the menu bar
    void showPopupMenu();
    
    // 
    // MenuBarModel
    // 

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& /*menuName*/) override;
    void menuItemSelected (int /*menuItemID*/, int /*topLevelMenuIndex*/) override;

  private:

    // demo did it this way not sure why
    //std::unique_ptr<juce::MenuBarComponent> menuBar;
    juce::MenuBarComponent menuBar;
    
    Listener* listener = nullptr;
    
    void popupSelection(int result);

};
