/**
 * A generator and manager of transient popup windows.
 *
 * These are similar to Panels, but fewer and require more
 * care.  Eventually I'd like to make it possible for any panel
 * to switch between panel or window rendering.
 */

#pragma once

#include "../script/ScriptRegistry.h"

class WindowFactory
{
  public:

    /**
     * Internal ids for windows.
     */
    enum WindowId {
        None,
        ScriptEditor,
        TraceLog
    };

    WindowFactory(class Supervisor* s);
    ~WindowFactory();

    /**
     * Show one of the panels, creating it if it does not
     * yet exist.
     */
    void show(WindowId id);

    /**
     * Named panels is used for bindings.
     */
    void show(juce::String name);

    /**
     * Hide a panel.
     * If this is a stateful editing panel the edit is not canceled.
     */
    void hide(WindowId id);

    /**
     * A periodic refresh ping from MainThread.
     */
    void update();
    
    void captureConfiguration(class UIConfig* config);
    
    void editScript(ScriptRegistry::File* file);
    
  private:

    class Supervisor* supervisor = nullptr;

    // demo used SafePointer because the child windows deleted themselves
    // don't necessarily need that here, but it's good to be safe
    juce::Component::SafePointer<class ScriptWindow> scriptEditor;

    void closeWindows();
    void showScriptEditor();
    void showTraceLog();
    //juce::Rectangle<int> getDisplayArea();
    
};
