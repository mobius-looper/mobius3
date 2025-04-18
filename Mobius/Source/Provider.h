/**
 * An interface for something that can provide application-wide utility objects.
 * This is implemented by Supervisor.
 *
 * Things that need things from Supervisor should reference this interface instead
 * so the definition of Supervisor.h can change without recompiling the whole damn world.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "Services.h"

class Provider : public FileChooserService
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

    /**
     * For a small number of components that want to receive high-resolution
     * refresh pings.
     */
    class HighRefreshListener {
      public:
        virtual ~HighRefreshListener() {}
        virtual void highRefresh(class PriorityState* state) = 0;
    };

    virtual ~Provider() {}

    virtual void addActionListener(ActionListener* l) = 0;
    virtual void removeActionListener(ActionListener* l) = 0;

    virtual void addAlertListener(AlertListener* l) = 0;
    virtual void removeAlertListener(AlertListener* l) = 0;

    virtual void addHighListener(HighRefreshListener* l) = 0;
    virtual void removeHighListener(HighRefreshListener* l) = 0;

    virtual class SystemConfig* getSystemConfig() = 0;
    virtual void updateSystemConfig() = 0;
    
    virtual class StaticConfig* getStaticConfig() = 0;
    virtual class Session* getSession() = 0;
    virtual class SymbolTable* getSymbols() = 0;
    virtual class MidiManager* getMidiManager() = 0;
    virtual class FileManager* getFileManager() = 0;
    virtual class MobiusInterface* getMobius() = 0;
    virtual int getSampleRate() = 0;

    // temporary until we work through how bindings are saved
    // and whether MCL should be dealing with Provider instead
    // of Supervisor
    virtual void mclBindingsUpdated() = 0;
    virtual void mclSessionUpdated() = 0;

    // controlled access to MobiusConfig
    virtual class MobiusConfig* getOldMobiusConfig() = 0;
    virtual class ParameterSets* getParameterSets() = 0;
    virtual class BindingSets* getBindingSets() = 0;
    virtual void updateParameterSets() = 0;
    virtual class GroupDefinitions* getGroupDefinitions() = 0;
    
    virtual class UIConfig* getUIConfig() = 0;
    virtual void updateUIConfig() = 0;

    virtual bool isPlugin() = 0;

    virtual void doAction(class UIAction*) = 0;
    virtual bool doQuery(class Query* q) = 0;

    virtual void showMainPopupMenu() = 0;
    
    virtual class MobiusView* getMobiusView() = 0;
    virtual class AudioClerk* getAudioClerk() = 0;

    virtual void loadMidi(int trackNumber, int loopNumber) = 0;
    virtual void saveMidi(int trackNumber, int loopNumber) = 0;
    virtual void dragMidi(int trackNumber, int loopNumber) = 0;
    virtual void loadAudio(int trackNumber, int loopNumber) = 0;
    virtual void saveAudio(int trackNumber, int loopNumber) = 0;
    virtual void dragAudio(int trackNumber, int loopNumber) = 0;

    virtual void addTemporaryFile(juce::TemporaryFile* tf) = 0;

    virtual class Pathfinder* getPathfinder() = 0;
    virtual class Prompter* getPrompter() = 0;
    virtual juce::File getRoot() = 0;
    virtual class ScriptClerk* getScriptClerk() = 0;
    virtual class Producer* getProducer() = 0;
    virtual class TaskMaster* getTaskMaster() = 0;
    
    virtual bool isTestMode() = 0;
    // this is terrible
    virtual bool isIdentifyMode() = 0;
    virtual int getActiveOverlay() = 0;
    virtual void getOverlayNames(juce::StringArray& names) = 0;
    
    // obscure things for Parametizer
    virtual class VariableManager* getVariableManager() = 0;
    virtual juce::AudioProcessor* getAudioProcessor() = 0;


    virtual void alert(juce::StringArray& messages) = 0;

    virtual int newUid() = 0;

    virtual class juce::Component* getDialogParent() = 0;
    
};
