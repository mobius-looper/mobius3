
#include <JuceHeader.h>

#include "BasePanel.h"
#include "MainWindow.h"
#include "JuceUtil.h"

#include "AboutPanel.h"
#include "EnvironmentPanel.h"
#include "MidiMonitorPanel.h"
#include "BindingSummaryPanel.h"

#include "../test/SymbolTablePanel.h"
#include "../test/MidiTransportPanel.h"
#include "../test/SyncPanel.h"
#include "../test/TracePanel.h"
#include "../test/UpgradePanel.h"
#include "../script/ConsolePanel.h"

// this has all the configuration panels
#include "config/ConfigPanels.h"

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

    switch (id) {

        case About: panel = new AboutPanel(); break;
        case Environment: panel = new EnvironmentPanel(); break;
        case MidiMonitor: panel = new MidiMonitorPanel(); break;
        case MidiSummary: panel = new MidiSummaryPanel(); break;
        case KeyboardSummary: panel = new KeyboardSummaryPanel(); break;

        case Global: panel = new GlobalPanel(); break;
        case Preset: panel = new PresetPanel(); break;
        case Setup: panel = new SetupPanel(); break;
        case Script: panel = new ScriptPanel(); break;
        case Sample: panel = new SamplePanel(); break;
        case Display: panel = new DisplayPanel(); break;
            
        case Keyboard: panel = new KeyboardPanel(); break;
        case Midi: panel = new MidiPanel(); break;
        case Button: panel = new ButtonPanel(); break;
        case Host: panel = new HostPanel(); break;

        case Audio: panel = new AudioPanel(); break;
        case MidiDevice: panel = new MidiDevicePanel(); break;

        case SymbolTable: panel = new SymbolTablePanel(); break;
        case MidiTransport: panel =  new MidiTransportPanel(); break;
        case Sync: panel =  new SyncPanel(); break;
        case Upgrade: panel =  new UpgradePanel(); break;
        case Console: panel = new ConsolePanel(); break;
        case TraceLog: panel = new TracePanel(); break;
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

    return id;
}

/**
 * Show a panel by name.
 */
void PanelFactory::show(juce::String name)
{
    PanelFactory::PanelId id =  mapPanelName(name);
    if (id != None)
      show(id);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
