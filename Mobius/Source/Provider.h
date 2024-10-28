/**
 * An interface for something that can provide application-wide utility objects.
 * This is implemented by Supervisor.
 *
 * Things that need things from Supervisor should reference this interface instead
 * so the definition of Supervisor.h can change without recompiling the whole damn world.
 *
 */

#pragma once

class Provider
{
  public:

    /**
     * Interface implemented by an internal component that wants
     * to handle UI level actions.  There are not many of these so
     * a listener style is easier than a "walk down" style.
     */
    class ActionListener {
      public:
        virtual ~ActionListener() {}
        virtual bool doAction(class UIAction* action) = 0;
    };

    /**
     * For display components that want to receive alerts.
     */
    class AlertListener {
      public:
        virtual ~AlertListener() {}
        virtual void alertReceived(juce::String msg) = 0;
    };

    virtual ~Provider() {}

    virtual void addActionListener(ActionListener* l) = 0;
    virtual void removeActionListener(ActionListener* l) = 0;

    virtual void addAlertListener(AlertListener* l) = 0;
    virtual void removeAlertListener(AlertListener* l) = 0;
    
    virtual class MobiusConfig* getMobiusConfig() = 0;
    virtual class Session* getSession() = 0;
    virtual class SymbolTable* getSymbols() = 0;
    virtual class MobiusMidiTransport* getMidiTransport() = 0;
    virtual class MidiRealizer* getMidiRealizer() = 0;
    virtual class MidiManager* getMidiManager() = 0;
    virtual int getSampleRate() = 0;

    virtual class UIConfig* getUIConfig() = 0;
    virtual void updateUIConfig() = 0;

    virtual void doAction(class UIAction*) = 0;
    virtual bool doQuery(class Query* q) = 0;

    virtual void showMainPopupMenu() = 0;
    
    virtual int getParameterMax(class Symbol* s) = 0;
    virtual juce::String getParameterLabel(class Symbol* s, int ordinal) = 0;
    virtual class MobiusView* getMobiusView() = 0;
    virtual class AudioClerk* getAudioClerk() = 0;

    // kludge for ctrl-click in the loop stack
    virtual void menuLoadMidi() = 0;
    
};
