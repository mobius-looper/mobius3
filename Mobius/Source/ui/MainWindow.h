/*
 * The main Mobius UI component.
 *
 * It combines a top-level menu (MainMenu) with a large central
 * status display area (MobiusDisplay) and a collection of on-demand
 * popup panels for editing the configuration properties (PanelFactory).
 *
 * From here down there must be no dependencies on the Juce Components
 * that we are contained in.
 *
 * When running as a standalone application, the parent will be a juce::AudioAppComponent.
 * When running as a plugin, the parent will be a juce::AudioProcessEditor.  Access to
 * things in the execution environment must be routed through Supervisor.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../Conditionals.h"
#include "MainMenu.h"
#include "display/MobiusDisplay.h"
#include "AlertPanel.h"
#include "PanelFactory.h"
#include "WindowFactory.h"

//#include "../ff_meters/ff_meters.h"

class MainWindow : public juce::Component, public MainMenu::Listener, public juce::FileDragAndDropTarget
{
  public:

    MainWindow(class Supervisor* super);
    ~MainWindow();

    class Supervisor* getSupervisor();
    class Provider* getProvider();

    void enableBigMeter();
        
    int getPreferredWidth();
    int getPreferredHeight();

    void resized() override;

    void configure();
    void update(class MobiusView* view);
    void captureConfiguration(class UIConfig* config);
    
    // MainMenu listener
    void mainMenuSelection(int id) override;

    // called by Supervisor to show the main popup menu
    void showMainPopupMenu();

    // called by Supervisor to show a panel
    void showPanel(juce::String name);
    
    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // kludge for StatusArea identify mode
    bool isIdentifyMode() {
        return display.isIdentifyMode();
    }

    void setIdentifyMode(bool b) {
        display.setIdentifyMode(b);
    }

    void alert(juce::String msg);

    // pop up a script editor
    void editScript(class ScriptRegistry::File* file);
    class ScriptWindow* openScriptWindow();
    
  private:

    juce::TooltipWindow tooltipWindow {this, 100};

    class Supervisor* supervisor = nullptr;

    MainMenu menu {this};
    MobiusDisplay display;
    AlertPanel alertPanel;
    PanelFactory panelFactory {this};
    WindowFactory windowFactory {supervisor};
    
    //foleys::LevelMeter levelMeter;
    
};    

