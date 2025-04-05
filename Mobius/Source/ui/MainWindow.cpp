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
#include "../model/UIAction.h"
#include "../model/Symbol.h"
#include "../model/SystemConfig.h"
#include "../model/BindingSets.h"
#include "../model/BindingSet.h"

#include "../Supervisor.h"
#include "../Symbolizer.h"
#include "../Producer.h"
#include "../Prompter.h"

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
}

// test hack
#if 0
void MainWindow::enableBigMeter()
{
    if (levelMeter.getParentComponent() == nullptr) {
        addAndMakeVisible(levelMeter);
        levelMeter.setMeterSource(supervisor->getLevelMeterSource());
    }
}
#endif

MainWindow::~MainWindow()
{
}

Supervisor* MainWindow::getSupervisor()
{
    return supervisor;
}
    
Provider* MainWindow::getProvider()
{
    return supervisor;
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
    
    //levelMeter.setBounds(20, 20, 200, 600);
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

/**
 * This handles both panels and windows so nothing
 * else needs to know the difference.  Should allow these
 * to configure themselves one way or the other.
 * Kludge on the name since we don't have a common namespace
 * or id mapping for these yet.
 */
void MainWindow::showPanel(juce::String name)
{
    if (!panelFactory.show(name)) {
        // wasn't a panel, try a window
        windowFactory.show(name);
    }
}

/**
 * Script editor is special because it takes an argument.
 * This is unlike panels which initialize themselves.
 * Need to refine generic ways to open things with arguments.
 */
void MainWindow::editScript(ScriptRegistry::File* file)
{
    windowFactory.editScript(file);
}

ScriptWindow* MainWindow::openScriptWindow()
{
    return windowFactory.openScriptWindow();
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
    else if (id >= MainMenu::MenuOverlayOffset && id <= MainMenu::MenuOverlayMax) {
        // overlays are 1 based with 0 meaning "none"
        // Supervisor will have injected "[None]" at the front
        int selected = id - MainMenu::MenuOverlayOffset;
        UIAction action;
        action.symbol = symbols->getSymbol(ParamTrackOverlay);
        action.value = selected;
        supervisor->doAction(&action);
    }
    else if (id >= MainMenu::MenuSessionOffset && id <= MainMenu::MenuSessionMax) {
        int ordinal = id - MainMenu::MenuSessionOffset;
        // sigh, this assumes that the Folder list won't change since the time the
        // menu was built till now, which is relatively safe, but it would be nicer
        // if we could get the session name here
        // not sure why I felt it necessary to send an action to Supervisor to change
        // things, but we can just call it
        supervisor->menuLoadSession(ordinal);
    }
    else if (id >= MainMenu::MenuLayoutOffset && id <= MainMenu::MenuLayoutMax) {
        int layoutOrdinal = id - MainMenu::MenuLayoutOffset;
        UIAction action;
        action.symbol = symbols->intern("activeLayout");
        action.value = layoutOrdinal;
        supervisor->doAction(&action);
    }
    else if (id >= MainMenu::MenuButtonsOffset && id <= MainMenu::MenuButtonsMax) {
        int buttonsOrdinal = id - MainMenu::MenuButtonsOffset;
        UIAction action;
        action.symbol = symbols->intern("activeButtons");
        action.value = buttonsOrdinal;
        supervisor->doAction(&action);
    }
    else if (id >= MainMenu::MenuBindingOffset && id <= MainMenu::MenuBindingMax) {
        // map this back into a particular BindingSet, sure would be nice to just
        // get the item name here
        // MainMenu left a kludgey transient menu id on the object
        SystemConfig* scon = supervisor->getSystemConfig();
        BindingSets* sets = scon->getBindings();
        if (sets != nullptr) {
            BindingSet* selected = nullptr;
            for (auto set : sets->getSets()) {
                if (set->transientMenuId == id) {
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

            case MainMenu::LoadMidi: {
                supervisor->menuLoadMidi(false);
            }
                break;
            case MainMenu::AnalyzeMidi: {
                supervisor->menuLoadMidi(true);
            }
                break;
            case MainMenu::RunMcl: {
                Prompter* p = supervisor->getPrompter();
                p->runMcl();
            }
                break;
            case MainMenu::Exit: {
                if (juce::JUCEApplicationBase::isStandaloneApp())
                  juce::JUCEApplicationBase::quit();
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
            case MainMenu::System: {
                panelFactory.show(PanelFactory::System);
            }
                break;
            case MainMenu::EditSession: {
                panelFactory.show(PanelFactory::Session);
            }
                break;
            case MainMenu::SessionManager: {
                panelFactory.show(PanelFactory::SessionManager);
            }
                break;
            case MainMenu::ReloadSession: {
                supervisor->menuReloadSession();
            }
                break;
            case MainMenu::Overlays: {
                panelFactory.show(PanelFactory::Overlay);
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

            case MainMenu::Bindings:
                panelFactory.show(PanelFactory::Bindings);
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

            case MainMenu::SymbolTable:
                panelFactory.show(PanelFactory::SymbolTable);
                break;
                
            case MainMenu::Console:
                panelFactory.show(PanelFactory::Console);
                break;
            case MainMenu::Monitor:
                panelFactory.show(PanelFactory::Monitor);
                break;
            case MainMenu::MclConsole:
                panelFactory.show(PanelFactory::MclConsole);
                break;
                
            case MainMenu::TraceLog:
                panelFactory.show(PanelFactory::TraceLog);
                break;
                
            case MainMenu::DecacheForms:
                panelFactory.decacheForms(PanelFactory::Session);
                supervisor->decacheForms();
                break;
                
            case MainMenu::InProgress:
                //panelFactory.show(PanelFactory::InProgress);
                panelFactory.show(PanelFactory::Buttons);
                break;
                
            case MainMenu::HelpTest:
                panelFactory.show(PanelFactory::HelpTest);
                break;
                
            case MainMenu::ScriptEditor:
                windowFactory.show(WindowFactory::ScriptEditor);
                break;
                
            case MainMenu::UpgradeConfig:
                panelFactory.show(PanelFactory::Upgrade);
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
    display.captureConfiguration(config);
    windowFactory.captureConfiguration(config);
}

/**
 * The periodic ping from MainThread to refresh the display.
 */
void MainWindow::update(class MobiusView* view)
{
    display.update(view);

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

/**
 * Respond to drops of script files and audio files.
 * Script files have an .msl or .mos extension.
 * AudioClerk should be more accepting, but currently just forwards
 * to Mobius which only reads .wav files.
 *
 * Targeting of specific tracks and loops is handled by more granular
 * drop targets, if we get them here, the mouse is over the main display area.
 * Since granular targets could be accidentally receivers of script files
 * figure out a way to chain them so if they find a script file they can forward
 * here, or factor out a file distributor all the targets can share.
 */

void MainWindow::filesDropped(const juce::StringArray& files, int x, int y)
{
    (void)x;
    (void)y;
    Trace(2, "MainWindow: filesDropped\n");
    
    juce::StringArray audioFiles;
    juce::StringArray scriptFiles;
    
    for (auto path : files) {
        juce::File f (path);
        juce::String ext = f.getFileExtension();

        if (ext == juce::String(".wav"))
          audioFiles.add(path);
        else if (ext == juce::String(".mid") || ext == juce::String(".smf")) {
            // a midi file
            // let AudioClerk handle the distribution since it also gets the files
            // dropped over the TrackStrip and LoopStack
            audioFiles.add(path);
        }
        else if (ext == juce::String(".mos") || ext == juce::String(".msl"))
          scriptFiles.add(path);
    }
    
    if (audioFiles.size() > 0) {
        AudioClerk* clerk = supervisor->getAudioClerk();
        clerk->filesDropped(audioFiles, 0, 0);
    }

    if (scriptFiles.size() > 0) {
        ScriptClerk* clerk = supervisor->getScriptClerk();
        clerk->filesDropped(scriptFiles);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
