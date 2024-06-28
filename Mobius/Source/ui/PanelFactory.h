/**
 * A generator and manager of transient popup panels.
 *
 * A single instance of this will live inside MainWindow and manage
 * the allocation, visibility, and cleanup of optional UI components
 * that behave similar to popup windows in traditional applications.
 *
 * Panels are shown over the main Mobius UI temporarily to display
 * information, or edit configuration.  They are allocated dynamically
 * to avoid startup overhead for panels that will not be used.  Once created
 * they are cached for reuse, and automatically disposed at shutdown.
 *
 * All panels will be subclasses of BasePanel.
 */

#pragma once

#include <JuceHeader.h>

class PanelFactory
{
  public:

    /**
     * All panels are identified by unique id internally.
     */
    enum PanelId {

        // informational panels
        About,
        Alert,
        MidiMonitor,
        Environment,
        MidiSummary,
        KeyboardSummary,

        // configuration editing panels
        Global,
        Setup,
        Preset,
        Button,
        Keyboard,
        Midi,
        MidiDevice,
        AudioDevice,
        Script,
        Sample,
        Display,
        PluginParameter,

        // testing and diagnostic panels
        SymbolTable,
        MidiTransport,
        Sync,
        Upgrade,
    };

    PanelFactory(class MainWindow* main);
    ~PanelFactory();

    /**
     * Show one of the panels, creating it if it does not
     * yet exist.
     */
    void show(PanelId id);

    /**
     * Hide a panel.
     * If this is a stateful editing panel the edit is not canceled.
     */
    void hide(PanelId id);

    /**
     * A periodic refresh ping from MainThread.
     */
    void update();

  private:

    class MainWindow* mainWindow = nullptr;
    juce::OwnedArray<BasePanel> panels;
    
    BasePanel* findPanel(PanelId id);
    BasePanel* createPanel(PanelId id);

};


    
