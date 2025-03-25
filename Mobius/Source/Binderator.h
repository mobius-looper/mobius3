/**
 * Class that manages the mapping between external events
 * and actions sent to the Mobius engine.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "model/UIAction.h"

#include "KeyTracker.h"
#include "MidiManager.h"

/**
 * Core class that consumes a MobiusConfig and builds out dispatch
 * tables to quickly map between an external event and a UIAction to send
 * to the UI or the engine.
 */
class Binderator
{
  public:

    // convert key codes and modifier bits to a compressed format
    // for storing in the Binding model, and in the Binderator jump table
    static unsigned int getKeyQualifier(const juce::KeyPress& kp);
    static unsigned int getKeyQualifier(int code, int modifiers);
    static void unpackKeyQualifier(int value, int* code, int* modifiers);
    
    static unsigned int getMidiQualifier(const juce::MidiMessage& msg);

    /**
     * Internal structure maintained in the action hash table to represent
     * collisions on the same table index.
     */
    struct TableEntry {

        // the action to perform
        class UIAction* action = nullptr;

        // for MIDI events, the qualifier is the channel number
        // for keyboard events, it is a combination of the full juce key code
        // and the modifier bits
        unsigned int qualifier = 0;

        // true if this is a release binding
        bool release = false;

        // true if this is associated with a release binding which means
        // that sustain/long abilities are not relevant
        bool hasRelease = false;

        // could use a smart pointer here, but it's really obvious what to do
        ~TableEntry() {
            delete action;
        }
        
    };

    Binderator();
    ~Binderator();

    /**
     * Construct mapping tables for both keyboard and MIDI events.
     */
    void configure(class SystemConfig* mc, class UIConfig* uc, class SymbolTable* st);
    
    /**
     * Construct mapping tables for only MIDI events.
     */
    void configureMidi(class SystemConfig* mc, class UIConfig* uc, class SymbolTable* st);

    /**
     * Construct mapping tables for only keyboard events.
     */
    void configureKeyboard(class SystemConfig* mc, class UIConfig* uc, class SymbolTable* st);

    /**
     * Map a MidiMessage to an action if one is defined and relevant.
     */
    class UIAction* handleMidiEvent(const juce::MidiMessage& message);
    
    /**
     * Map a key evevnt to an action.
     */
    class UIAction* handleKeyEvent(int code, int modifiers, bool up);

  private:

    int controllerThreshold = 0;
    juce::OwnedArray<juce::OwnedArray<TableEntry>> keyActions;
    juce::OwnedArray<juce::OwnedArray<TableEntry>> noteActions;
    juce::OwnedArray<juce::OwnedArray<TableEntry>> programActions;
    juce::OwnedArray<juce::OwnedArray<TableEntry>> controlActions;

    void prepareArray(juce::OwnedArray<juce::OwnedArray<TableEntry>>* table);
    void addEntry(juce::OwnedArray<juce::OwnedArray<TableEntry>>* table,
                  int hashKey, unsigned int qualifier, bool release, UIAction* action);
    UIAction* getAction(juce::OwnedArray<juce::OwnedArray<TableEntry>>* table,
                        int hashKey, unsigned int qualifier, bool release, bool wildZero);

    void installKeyboardActions(class SystemConfig* sconfig, class UIConfig* uconfig,
                                class SymbolTable* symbols);
    void installMidiActions(class SystemConfig* sconfig, class UIConfig* uconfig,
                            class SymbolTable* symbols);
    void installMidiActions(class SymbolTable* symbols, class BindingSet* set);
    class UIAction* buildAction(class SymbolTable* symbols, class Binding* b);
    
    class UIAction* getKeyAction(int code, int modifiers, bool release);
    class UIAction* getMidiAction(const class juce::MidiMessage& msg);
    
    bool looksResolved(class Symbol* s);
};

/**
 * Binderator wrapper that provides a receiver for keyboard and
 * MIDI events when running as a standalone application.
 */
class ApplicationBinderator : public KeyTracker::Listener,
                              public MidiManager::Listener
{
  public:

    ApplicationBinderator(class Supervisor* super);
    ~ApplicationBinderator();

    void configure(class SystemConfig* sc, class UIConfig* uc, class SymbolTable* st);
    void configureKeyboard(class SystemConfig* sc, class UIConfig* uc, class SymbolTable* st);
    void start();
    void stop();
    
    void keyTrackerDown(int code, int modifiers) override;
    void keyTrackerUp(int code, int modifiers) override;

    void midiMessage(const class juce::MidiMessage& message, juce::String& source) override;

  private:

    class Supervisor* supervisor = nullptr;
    Binderator binderator;
    bool started = false;
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
