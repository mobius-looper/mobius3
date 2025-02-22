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

        // used in name mapping to indiciate invalid name
        None,

        // informational panels
        About,
        Alert,
        MidiMonitor,
        Environment,
        MidiSummary,
        KeyboardSummary,

        // configuration editing panels
        Preset,
        Keyboard,
        Midi,
        Host,
        Script,
        Sample,
        Display,
        Button,
        Properties,
        Group,
        Session,
        SessionManager,
        ParameterSets,

        // Devices
        Audio,
        MidiDevice,

        // Scripts
        Console,
        Monitor,
        
        // testing and diagnostic panels
        SymbolTable,
        Upgrade,
        TraceLog
    };

    PanelFactory(class MainWindow* main);
    ~PanelFactory();

    /**
     * Show one of the panels, creating it if it does not
     * yet exist.
     */
    void show(PanelId id);

    /**
     * Named panels is used for bindings.
     */
    bool show(juce::String name);

    /**
     * Hide a panel.
     * If this is a stateful editing panel the edit is not canceled.
     */
    void hide(PanelId id);

    /**
     * A periodic refresh ping from MainThread.
     */
    void update();

    /**
     * Development tool to decache TreeForms held by one of the panels.
     */
    void decacheForms(PanelId id);

  private:

    class MainWindow* mainWindow = nullptr;
    juce::OwnedArray<BasePanel> panels;
    
    BasePanel* findPanel(PanelId id);
    BasePanel* createPanel(PanelId id);
    PanelId mapPanelName(juce::String name);

};


    
