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

    class MobiusConfig* mobiusConfig = nullptr;
    class UIConfig* uiConfig = nullptr;
    class MobiusConfig* masterConfig = nullptr;

    juce::OwnedArray<class Preset> newPresets;
    juce::OwnedArray<class Setup> newSetups;
    juce::OwnedArray<class ScriptRef> newScripts;
    juce::Array<juce::String> scriptNames;
    juce::OwnedArray<class Binding> newBindings;
    
    BasicLog log;

    BasicButtonRow commands;
    juce::TextButton loadButton {"Load File"};
    juce::TextButton installButton {"Install"};
    juce::TextButton undoButton {"Undo"};
    
    UpgradePanelFooter footer;
    juce::TextButton okButton {"OK"};
    
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFolder;

    void doLoad();
    void doFileChooser();
    void doLoad(juce::File file);

    void loadUIConfig(juce::String xml);
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

    void doInstall();
    void noLoad();
    
    void doUndo();
};

