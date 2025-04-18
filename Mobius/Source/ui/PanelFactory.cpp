
#include <JuceHeader.h>

#include "BasePanel.h"
#include "MainWindow.h"
#include "JuceUtil.h"

#include "AboutPanel.h"
#include "EnvironmentPanel.h"
#include "MidiMonitorPanel.h"
#include "BindingSummaryPanel.h"

#include "../test/SymbolTablePanel.h"
#include "../test/TracePanel.h"
#include "../test/UpgradePanel.h"

#include "script/ConsolePanel.h"
#include "script/MonitorPanel.h"
#include "script/MclPanel.h"

#include "session/SessionManagerPanel.h"

// this has all the configuration panels
#include "config/ConfigPanels.h"

#include "binding/NewBindingPanel.h"
#include "binding/NewButtonPanel.h"
#include "help/HelpTest.h"

#include "PanelFactory.h"

PanelFactory::PanelFactory(MainWindow* main)
{
    mainWindow = main;
}

/**
 * Panels are maintained in an OwnedArray so are deleted
 * automatically.  Would we need to do any pre-delete
 * cleanup here?
 */
PanelFactory::~PanelFactory()
{
}

void PanelFactory::show(PanelId id)
{
    BasePanel* panel = findPanel(id);
    bool isNew = false;
    
    if (panel == nullptr) {
        isNew = true;
        panel = createPanel(id);
        if (panel != nullptr) {
            panels.add(panel);
            mainWindow->addChildComponent(panel);
        }
    }

    if (panel != nullptr) {

        if (!panel->isVisible()) {

            panel->showing();

            if (isNew)
              JuceUtil::centerInParent(panel);

            panel->setVisible(true);
        }

        // whether previously visible or not,
        // always move it to the top and give it focus
        panel->toFront(true);
    }
}

/**
 * Force a panel to become hidden.
 * Usually panels hide themselves.
 */
void PanelFactory::hide(PanelId id)
{
    BasePanel* panel = findPanel(id);
    if (panel != nullptr) {

        if (panel->isVisible()) {
            panel->hiding();
            panel->setVisible(false);
        }
    }
}

void PanelFactory::decacheForms(PanelId id)
{
    BasePanel* panel = findPanel(id);
    if (panel != nullptr)
      panel->decacheForms();
}

/**
 * Here via MainThread->Supervisor->MainWindow with the
 * periodic refresh ping.  There aren't many that need
 * periodic refresh, SyncPanel is one.
 */
void PanelFactory::update()
{
    for (auto panel : panels) {
        if (panel->isVisible())
          panel->update();
    }
}

BasePanel* PanelFactory::findPanel(PanelId id)
{
    BasePanel* found = nullptr;

    for (auto panel : panels) {
        if (panel->getId() == id) {
            found = panel;
            break;
        }
    }

    return found;
}

BasePanel* PanelFactory::createPanel(PanelId id)
{
    BasePanel* panel = nullptr;
    Supervisor* super = mainWindow->getSupervisor();
    
    switch (id) {

        case About: panel = new AboutPanel(super); break;
        case Environment: panel = new EnvironmentPanel(super); break;
        case MidiMonitor: panel = new MidiMonitorPanel(super); break;
        case MidiSummary: panel = new MidiSummaryPanel(super); break;
        case KeyboardSummary: panel = new KeyboardSummaryPanel(super); break;

        case Script: panel = new ScriptPanel(super); break;
        case Sample: panel = new SamplePanel(super); break;
        case Display: panel = new DisplayPanel(super); break;

        case Bindings: panel = new NewBindingPanel(super); break;
        case Buttons: panel = new NewButtonPanel(super); break;
        case Properties: panel = new PropertiesPanel(super); break;
        case Group: panel = new GroupPanel(super); break;
        case Session: panel = new SessionPanel(super); break;
        case SessionManager: panel = new SessionManagerPanel(super); break;
        case Overlay: panel = new OverlayPanel(super); break;
        case System: panel = new SystemPanel(super); break;
            
        case Audio: panel = new AudioPanel(super); break;
        case MidiDevice: panel = new MidiDevicePanel(super); break;

        case SymbolTable: panel = new SymbolTablePanel(super); break;
        case Upgrade: panel =  new UpgradePanel(super); break;
        case Console: panel = new ConsolePanel(super); break;
        case Monitor: panel = new MonitorPanel(super); break;
        case MclConsole: panel = new MclPanel(super); break;
        case TraceLog: panel = new TracePanel(super); break;
            
        case InProgress: break;
        case HelpTest: panel = new HelpPanel(super); break;
            
        default:
            Trace(1, "PanelFactory: Unknown panel id %d\n", id);
            break;
    }

    if (panel != nullptr) {
        panel->setId(id);
    }

    return panel;
}
            
/**
 * Panel name to id mapping.
 * This is used when the panel is to be brought up under the
 * control of a binding or script where the user wants to deal with
 * the name rather than the id.
 *
 * We don't have a good registry for these, just hard code the few
 * that I want all the time.  Each BasePanel class should overload
 * a getBindingName or something to publish the names to use.
 */
PanelFactory::PanelId PanelFactory::mapPanelName(juce::String name)
{
    PanelFactory::PanelId id = None;

    if (name == "console")
      id = Console;
    else if (name == "monitor")
      id = Monitor;

    return id;
}

/**
 * Show a panel by name.
 */
bool PanelFactory::show(juce::String name)
{
    bool found = false;
    
    PanelFactory::PanelId id =  mapPanelName(name);
    if (id != None) {
        show(id);
        found = true;
    }
    return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
