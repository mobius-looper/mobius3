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
#include "../model/MobiusConfig.h"
#include "../model/Binding.h"

#include "../Supervisor.h"
#include "../Symbolizer.h"

#include "JuceUtil.h"
#include "MainMenu.h"

#include "MainWindow.h"

MainWindow::MainWindow(Supervisor* super) : supervisor(super), display(this), alertPanel(super) 
{
    setName("MainWindow");

    // using a Listener pattern here but could just
    // pass this to the constructor like we do for the others
    addAndMakeVisible(menu);
    menu.setListener(this);
    
    addAndMakeVisible(display);
    addChildComponent(alertPanel);
    
#ifdef USE_FFMETERS
    addAndMakeVisible(levelMeter);
    levelMeter.setMeterSource(supervisor->getLevelMeterSource());
#endif
    
}

MainWindow::~MainWindow()
{
}

/**
 * Inform child components of configuration changes.
 * The various PanelFactory popup panels are not currently sensitive.
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

void MainWindow::alert(juce::String msg)
{
    alertPanel.show(msg);
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

void MainWindow::showPanel(juce::String name)
{
    panelFactory.show(name);
}

//////////////////////////////////////////////////////////////////////
//
// Menu Callbacks
//
//////////////////////////////////////////////////////////////////////

void MainWindow::mainMenuSelection(int id)
{
    SymbolTable* symbols = supervisor->getSymbols();
    
    if (id == 0) {
        // can get here when using the PopupMenu
        // this means that the user released the mouse
        // without selecting an item
    }
    else if (id >= MainMenu::MenuPresetOffset && id <= MainMenu::MenuPresetMax) {
        int preset = id - MainMenu::MenuPresetOffset;
        UIAction action;
        action.symbol = symbols->intern("activePreset");
        action.value = preset;
        supervisor->doAction(&action);
    }
    else if (id >= MainMenu::MenuSetupOffset && id <= MainMenu::MenuSetupMax) {
        int setup = id - MainMenu::MenuSetupOffset;
        UIAction action;
        action.symbol = symbols->intern("activeSetup");
        action.value = setup;
        supervisor->doAction(&action);
    }
    else if (id >= MainMenu::MenuLayoutOffset && id <= MainMenu::MenuLayoutMax) {
        int layoutOrdinal = id - MainMenu::MenuLayoutOffset;
        UIAction action;
        action.symbol = symbols->intern(UISymbols::ActiveLayout);
        action.value = layoutOrdinal;
        supervisor->doAction(&action);
    }
    else if (id >= MainMenu::MenuButtonsOffset && id <= MainMenu::MenuButtonsMax) {
        int buttonsOrdinal = id - MainMenu::MenuButtonsOffset;
        UIAction action;
        action.symbol = symbols->intern(UISymbols::ActiveButtons);
        action.value = buttonsOrdinal;
        supervisor->doAction(&action);
    }
    else if (id >= MainMenu::MenuBindingOffset && id <= MainMenu::MenuBindingMax) {
        int index = id - MainMenu::MenuButtonsOffset;
        // map this back into a particular BindingSet, sure would be nice to just
        // get the item name here
        // MainMenu left a kludgey transient menu id on the object
        MobiusConfig* mconfig = supervisor->getMobiusConfig();
        BindingSet* sets = mconfig->getBindingSets();
        BindingSet* selected = nullptr;
        for (BindingSet* set = sets ; set != nullptr ; set = set->getNextBindingSet()) {
            if (set->transientMenuId == index) {
                selected = set;
                break;
            }
        }

        // now we've worked our way back to a BindingSet, punt to Supervisor
        if (selected == nullptr)
          Trace(1, "MainWindow: BindingSet resolution failed, and so have you");
        else
          supervisor->menuActivateBindings(selected);
    }
    else {
        switch (id) {
            case MainMenu::OpenLoop:
                supervisor->menuLoadLoop();
                break;
            case MainMenu::OpenProject:
                supervisor->menuLoadProject();
                break;
            case MainMenu::SaveLoop:
                supervisor->menuSaveLoop();
                break;
            case MainMenu::SaveProject:
                supervisor->menuSaveProject();
                break;
            case MainMenu::QuickSave:
                supervisor->menuQuickSave();
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
                panelFactory.show(PanelFactory::Global);
            }
                break;
            case MainMenu::Properties: {
                panelFactory.show(PanelFactory::Properties);
            }
                break;
            case MainMenu::Groups: {
                panelFactory.show(PanelFactory::Group);
            }
                break;
            case MainMenu::Presets: {
                panelFactory.show(PanelFactory::Preset);
            }
                break;
            case MainMenu::TrackSetups: {
                panelFactory.show(PanelFactory::Setup);
            }
                break;
            case MainMenu::MidiControl: {
                panelFactory.show(PanelFactory::Midi);
            }
                break;
        
            case MainMenu::KeyboardControl:  {
                panelFactory.show(PanelFactory::Keyboard);
            }
                break;
            
            case MainMenu::Buttons: {
                panelFactory.show(PanelFactory::Button);
            }
                break;

            case MainMenu::HostParameters: {
                panelFactory.show(PanelFactory::Host);
            }
                break;

            case MainMenu::DisplayComponents: {
                panelFactory.show(PanelFactory::Display);
            }
                break;
            case MainMenu::Scripts: {
                panelFactory.show(PanelFactory::Script);
            }
                break;
            case MainMenu::Samples: {
                panelFactory.show(PanelFactory::Sample);
            }
                break;
            case MainMenu::MidiDevices: {
                panelFactory.show(PanelFactory::MidiDevice);
            }
                break;
            case MainMenu::AudioDevices: {
                // can only show this if we're standalone
                if (!supervisor->isPlugin())
                  panelFactory.show(PanelFactory::Audio);
            }
                break;

            case MainMenu::KeyBindings:
                panelFactory.show(PanelFactory::KeyboardSummary);
                break;
            case MainMenu::MidiBindings:
                panelFactory.show(PanelFactory::MidiSummary);
                break;
            case MainMenu::MidiMonitor:
                panelFactory.show(PanelFactory::MidiMonitor);
                break;
            case MainMenu::Environment:
                panelFactory.show(PanelFactory::Environment);
                break;
                
            case MainMenu::MidiTransport:
                panelFactory.show(PanelFactory::MidiTransport);
                break;
                
            case MainMenu::SyncPanel:
                panelFactory.show(PanelFactory::Sync);
                break;
                
            case MainMenu::SymbolTable:
                panelFactory.show(PanelFactory::SymbolTable);
                break;
                
            case MainMenu::Console:
                panelFactory.show(PanelFactory::Console);
                break;
                
            case MainMenu::TraceLog:
                panelFactory.show(PanelFactory::TraceLog);
                break;
                
            case MainMenu::UpgradeConfig:
                panelFactory.show(PanelFactory::Upgrade);
                break;
                
            case MainMenu::DiagnosticWindow: {
                // never did work
                //DiagnosticWindow::launch();
            }
                break;

            case MainMenu::About: {
                panelFactory.show(PanelFactory::About);
            }
                break;

            case MainMenu::TestInfo: {
                supervisor->alert("The test menu has development tools that will be hidden in normal releases.  They don't do anything partiuclarly useful.  You probably won't hurt anything if you use them.  Probably.");
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

/**
 * The periodic ping from MainThread to refresh the display.
 */
void MainWindow::update(MobiusState* state)
{
    display.update(state);

    // A few visible panels need to do periodic refreshes as well
    panelFactory.update();
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
