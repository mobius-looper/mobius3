/**
 * Modest utility to upgrade and merge old Mobius 2 configuration
 * files into the new one.
 */

#pragma once

#include "BasicLog.h"

class UpgradePanelFooter : public juce::Component
{
  public:
    UpgradePanelFooter() {}
    ~UpgradePanelFooter() {}
};

class UpgradePanel : public juce::Component, juce::Button::Listener
{
  public:

    UpgradePanel();
    ~UpgradePanel();

    void show();

    void resized() override;
    void paint(juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;

  private:

    bool strict = false;
    juce::File expected;
    bool expectedVerified = false;
    
    class MobiusConfig* mobiusConfig = nullptr;
    class MobiusConfig* masterConfig = nullptr;

    juce::OwnedArray<class Preset> newPresets;
    juce::OwnedArray<class Setup> newSetups;
    juce::OwnedArray<class ScriptRef> newScripts;
    juce::Array<juce::String> scriptNames;
    juce::OwnedArray<class Binding> newBindings;
    class ButtonSet* newButtons = nullptr;
    
    BasicLog log;

    BasicButtonRow commands;
    juce::TextButton loadButton {"Load File"};
    juce::TextButton installButton {"Install"};
    juce::TextButton undoButton {"Undo"};
    
    UpgradePanelFooter footer;
    juce::TextButton okButton {"OK"};
    
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFolder;

    void locateExisting();
    void doLoad();
    void doFileChooser();
    void doLoad(juce::File file);

    void loadUIConfig(juce::String xml);
    bool convertButton(juce::XmlElement* el);
    
    void loadMobiusConfig(juce::String xml);
    void loadPresets();
    void loadSetups();
    void loadScripts();
    juce::String verifyScript(ScriptRef* ref);
    juce::String getScriptNameFromPath(const char* path);
    void registerDirectoryScripts(juce::File dir);
    juce::String getScriptName(juce::File file);
    
    void loadBindings();
    Binding* verifyBinding(Binding* src);
    int getLastDigit(juce::String s);
    bool addUpgradeButton(class DisplayButton* db);

    void doInstall();
    void noLoad();
    
    void doUndo();
};

