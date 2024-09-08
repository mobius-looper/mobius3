/**
 * Similar in purpose to PanelFactory but manages autonomous popup windows.
 */

#include <JuceHeader.h>

#include "../model/UIConfig.h"

#include "../Supervisor.h"

#include "MainWindow.h"
#include "JuceUtil.h"

#include "script/ScriptWindow.h"

#include "WindowFactory.h"

WindowFactory::WindowFactory(Supervisor* s)
{
    supervisor = s;
}

/**
 * Panels are maintained in an OwnedArray so are deleted
 * automatically.  Would we need to do any pre-delete
 * cleanup here?
 */
WindowFactory::~WindowFactory()
{
    // PanelFactory maintains panels in an owned array
    // we have to do it manually
    closeWindows();
}

void WindowFactory::closeWindows()
{
    scriptEditor.deleteAndZero();
}

void WindowFactory::captureConfiguration(UIConfig* config)
{
    config->captureLocations(supervisor->getMainWindow(), scriptEditor);
}

void WindowFactory::show(WindowId id)
{
    if (id == ScriptEditor) {
        showScriptEditor();
    }
    else if (id == TraceLog) {
        showTraceLog();
    }
}

/**
 * Show a panel by name.
 */
void WindowFactory::show(juce::String name)
{
    // todo: need a real name-to-id mapping table if this gets big
    if (name == "scriptEditor") {
        show(ScriptEditor);
    }
    // "traceLog" is used by PanelFactory so until
    // we can put the panel/window configuration under one name
    // have to use two names
    else if (name == "traceWindow") {
        show(TraceLog);
    }
}

/**
 * Force a panel to become hidden.
 * Usually panels hide themselves.
 */
void WindowFactory::hide(WindowId id)
{
    (void)id;
    // todo
    Trace(1, "WindowFactory::hide not implemented");
}

/**
 * Here via MainThread->Supervisor->MainWindow with the
 * periodic refresh ping.
 */
void WindowFactory::update()
{
}

//////////////////////////////////////////////////////////////////////
//
// Specific Windows
//
// Not as easily extensible as WindowFactory but enough for now
//
//////////////////////////////////////////////////////////////////////

/**
 * Here via MainWindow which is what everything calls in order to
 * hide ScriptEditor.  ScriptEditor is unlike Panels because it
 * takes an argument.  Think about ways to generlaize opening
 * a window (or panel) with arbitrary window specific arguments.
 */
void WindowFactory::editScript(ScriptRegistry::File* file)
{
    showScriptEditor();
    scriptEditor->load(file);
}

void WindowFactory::showScriptEditor()
{
    if (scriptEditor == nullptr) {
        ScriptWindow* w = new ScriptWindow(supervisor);
        scriptEditor = juce::Component::SafePointer<ScriptWindow>(w);

        UIConfig* config = supervisor->getUIConfig();
        UILocation loc = config->getScriptWindowLocation();
        juce::Rectangle<int> bounds = scriptEditor->getBounds();
        loc.adjustBounds(bounds);
        scriptEditor->setBounds(bounds);
    }

    scriptEditor->setVisible(true);
}

void WindowFactory::showTraceLog()
{
}


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
