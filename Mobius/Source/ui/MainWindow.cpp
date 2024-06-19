/*
 * The default Mobius main window.
 *
 * Contains a main menu, a set of configuration editor popup panels
 * and the MobiusDisplay.
 *
 * This doesn't do much besides organizing the few primary components,
 * being the MainMenu::Listener which forwards most things ot Supervisor.
 * It also handles file drag and drop.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/UIConfig.h"
#include "../model/MobiusState.h"
#include "../model/UIAction.h"
#include "../model/Symbol.h"

//#include "../DiagnosticWindow.h"
#include "../Supervisor.h"
#include "../UISymbols.h"

#include "JuceUtil.h"
#include "MainMenu.h"

#include "config/ConfigEditor.h"

#include "MainWindow.h"

MainWindow::MainWindow(Supervisor* super)
{
    setName("MainWindow");
    supervisor = super;

    // using a Listener pattern here but could just
    // pass this to the constructor like we do for the others
    addAndMakeVisible(menu);
    menu.setListener(this);
    
    addAndMakeVisible(display);

    // let the config editor pre-load all its panels
    configEditor.init(supervisor);

    addChildComponent(infoPanel);
    addChildComponent(aboutPanel);
    addChildComponent(alertPanel);
    addChildComponent(midiMonitor);
    
#ifdef USE_FFMETERS
    addAndMakeVisible(levelMeter);
    levelMeter.setMeterSource(supervisor->getLevelMeterSource());
#endif
    
}

MainWindow::~MainWindow()
{
}

/**
 * Inform children of a change to the configuration
 *
 * MainMenu, ConfigEditor, InfoPanel, AboutPanel are not
 * sensitive to configuration changes.
 */
void MainWindow::configure()
{
    display.configure();
}

/**
 * Called by a Supervisor when a child component received a MouseEvent
 * and wants to display the main popup menu.  MainWindow can't
 * receive mouse events because it is completely covered by children
 * and mouse events always go to the child component the mouse is over.
 *
 * It isn't possible (easily) to process mouse events top-down, with each
 * successive level deciding whether to propagate the event to the children,
 * you normally do this bottom up.  Since I have few popup menus, rather than
 * implement mouse event methods at every level of the hierarchy, a child component
 * can just call up to Supervisor to get the menu, and it can do the showMenuAsync.
 * If you do start passsing up MouseEvents, remember to adjust the event coordinates
 * at each level since they will be relative to the child that originally received
 * the event.
 * See notes/ui-mouse.txt for more
 */
void MainWindow::showMainPopupMenu()
{
    menu.showPopupMenu();
}

void MainWindow::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    menu.setBounds(area.removeFromTop(menu.getPreferredHeight()));
    display.setBounds(area);

#ifdef USE_FFMETERS
    levelMeter.setBounds(20, 20, 200, 600);
#endif
}

/**
 * Start with a reasonable size, in time, can query the subcomponents
 * to determine optimal miniminum size, but that may need to be deferred
 * until after configuration.
 */
int MainWindow::getPreferredWidth()
{
    return 1200;
}

int MainWindow::getPreferredHeight()
{
    return 800;
}

//////////////////////////////////////////////////////////////////////
//
// Menu Callbacks
//
//////////////////////////////////////////////////////////////////////

void MainWindow::mainMenuSelection(int id)
{
    if (id == 0) {
        // can get here when using the PopupMenu
        // this means that the user released the mouse
        // without selecting an item
    }
    else if (id >= MainMenu::MenuPresetOffset && id <= MainMenu::MenuPresetMax) {
        int preset = id - MainMenu::MenuPresetOffset;
        UIAction action;
        action.symbol = Symbols.intern("activePreset");
        action.value = preset;
        supervisor->doAction(&action);
    }
    else if (id >= MainMenu::MenuSetupOffset && id <= MainMenu::MenuSetupMax) {
        int setup = id - MainMenu::MenuSetupOffset;
        UIAction action;
        action.symbol = Symbols.intern("activeSetup");
        action.value = setup;
        supervisor->doAction(&action);
    }
    else if (id >= MainMenu::MenuLayoutOffset && id <= MainMenu::MenuLayoutMax) {
        int layoutOrdinal = id - MainMenu::MenuLayoutOffset;
        UIAction action;
        action.symbol = Symbols.intern(UISymbols::ActiveLayout);
        action.value = layoutOrdinal;
        supervisor->doAction(&action);
    }
    else if (id >= MainMenu::MenuButtonsOffset && id <= MainMenu::MenuButtonsMax) {
        int buttonsOrdinal = id - MainMenu::MenuButtonsOffset;
        UIAction action;
        action.symbol = Symbols.intern(UISymbols::ActiveButtons);
        action.value = buttonsOrdinal;
        supervisor->doAction(&action);
    }
    else {
        switch (id) {
            case MainMenu::OpenLoop:
                supervisor->alert("Open Loop not implemented");
                break;
            case MainMenu::OpenProject: 
                supervisor->alert("Open Project not implemented");
                break;
            case MainMenu::SaveLoop: 
                supervisor->alert("Save Loop not implemented");
                break;
            case MainMenu::SaveProject:
                supervisor->alert("Save Project not implemented");
                break;
            case MainMenu::QuickSave:
                supervisor->alert("Quick Save not implemented");
                break;
                
            case MainMenu::LoadScripts: {
                supervisor->menuLoadScripts();
            }
                break;
            case MainMenu::LoadSamples: {
                supervisor->menuLoadSamples();
            }
                break;

            case MainMenu::Exit: {
                if (juce::JUCEApplicationBase::isStandaloneApp())
                  juce::JUCEApplicationBase::quit();
            }
                break;

            case MainMenu::GlobalParameters: {
                configEditor.showGlobal();
            }
                break;
            case MainMenu::Presets: {
                configEditor.showPresets();
            }
                break;
            case MainMenu::TrackSetups: {
                configEditor.showSetups();
            }
                break;
            case MainMenu::MidiControl: {
                configEditor.showMidiBindings();
            }
                break;
        
            case MainMenu::KeyboardControl:  {
                configEditor.showKeyboardBindings();
            }
                break;
            
            case MainMenu::Buttons: {
                configEditor.showButtons();
            }
                break;

            case MainMenu::HostParameters: {
                configEditor.showHostParameters();
            }
                break;

            case MainMenu::DisplayComponents: {
                configEditor.showDisplay();
            }
                break;
            case MainMenu::Scripts: {
                configEditor.showScripts();
            }
                break;
            case MainMenu::Samples: {
                configEditor.showSamples();
            }
                break;
            case MainMenu::MidiDevices: {
                configEditor.showMidiDevices();
            }
                break;
            case MainMenu::AudioDevices: {
                // can only show this if we're standalone
                if (!supervisor->isPlugin())
                  configEditor.showAudioDevices();
            }
                break;

            case MainMenu::KeyBindings:
                infoPanel.showKeyboard();
                break;
            case MainMenu::MidiBindings: 
                infoPanel.showMidi();
                break;
            case MainMenu::MidiMonitor:
                midiMonitor.show();
                break;
                
            case MainMenu::MidiTransport:
                supervisor->menuShowMidiTransport();
                break;
                
            case MainMenu::SyncPanel:
                supervisor->menuShowSyncPanel();
                break;
                
            case MainMenu::SymbolTable:
                supervisor->menuShowSymbolTable();
                break;
                
            case MainMenu::UpgradeConfig:
                supervisor->menuShowUpgradePanel();
                break;
                
            case MainMenu::DiagnosticWindow: {
                // never did work
                //DiagnosticWindow::launch();
            }
                break;
                
            case MainMenu::About: {
                aboutPanel.show();
            }
                break;

            case MainMenu::TestInfo: {
                Supervisor::Instance->alert("The test menu has development tools that will be hidden in normal releases.  They don't do anything partiuclarly useful.  You probably won't hurt anything if you use them.  Probably.");
            }
                break;

            case MainMenu::TestMode: {
                supervisor->menuTestMode();
            }
                break;

            case MainMenu::MenuOptionsBorders: {
                UIConfig* config = supervisor->getUIConfig();
                config->showBorders = !config->showBorders;
                // todo: poke StatusArea and make it redraw
                Trace(2, "MainWindow: MenuOptionsBorders %d\n", (int)config->showBorders);
                supervisor->propagateConfiguration();
            }
                break;
                
            case MainMenu::MenuOptionsIdentify: {
                supervisor->setIdentifyMode(!supervisor->isIdentifyMode());
            }
                break;
                
            default: {
                Trace(1, "MainWindow: Unknown menu item: %d\n", id);
            }
                break;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

/**
 * Called during the shutdown process to save any accumulated
 * changes.
 */
void MainWindow::captureConfiguration(UIConfig* config)
{
    config->windowWidth = getWidth();
    config->windowHeight = getHeight();
    // todo: should we save the screen origin X/Y?
    
    display.captureConfiguration(config);
}

void MainWindow::update(MobiusState* state)
{
    display.update(state);
}

//////////////////////////////////////////////////////////////////////
//
// FileDragAndDropTarget
//
//////////////////////////////////////////////////////////////////////

/**
 * "Callback to check whether this target is interested in the set of files being offered.
 *
 * Note that this will be called repeatedly when the user is dragging the mouse around
 * over your component, so don't do anything time-consuming in here, like opening the
 * files to have a look inside them!"
 */
bool MainWindow::isInterestedInFileDrag(const juce::StringArray& files)
{
    (void)files;
    return true;
}

void MainWindow::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    (void)files;
    (void)x;
    (void)y;
    //Trace(2, "MainWindow: fileDragEnter\n");
}

void MainWindow::fileDragMove(const juce::StringArray& files, int x, int y)
{
    (void)files;
    (void)x;
    (void)y;
    //Trace(2, "MainWindow: fileDragMove\n");
}

void MainWindow::fileDragExit(const juce::StringArray& files)
{
    (void)files;
    //Trace(2, "MainWindow: fileDragExit\n");
}

void MainWindow::filesDropped(const juce::StringArray& files, int x, int y)
{
    (void)files;
    (void)x;
    (void)y;
    Trace(2, "MainWindow: filesDropped\n");

    // todo: for now just pass the files to AudioClerk and let
    // it figure out where they go.  Would be nice to look at the
    // x/y location and allow specific tracks/loops to be targeted
    // if you drop over the track strip or loop stack
    
    AudioClerk* clerk = supervisor->getAudioClerk();
    clerk->filesDropped(files, 0, 0);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
