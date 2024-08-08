/**
 * Modest utility to upgrade and merge old Mobius 2 configuration
 * files into the new one.
 */

#pragma once

#include "../ui/BasePanel.h"
#include "../ui/common/BasicLog.h"
#include "../ui/common/BasicButtonRow.h"

class UpgradeContent : public juce::Component, juce::Button::Listener
{
  public:

    UpgradeContent(class Supervisor* s);
    ~UpgradeContent();

    void showing();

    void resized() override;
    void buttonClicked(juce::Button* b) override;

  private:
    
    class Supervisor* supervisor = nullptr;
    bool strict = false;
    juce::File expected;
    bool expectedVerified = false;
    
    class MobiusConfig* mobiusConfig = nullptr;
    class MobiusConfig* masterConfig = nullptr;

    juce::OwnedArray<class Preset> newPresets;
    juce::OwnedArray<class Setup> newSetups;
    juce::OwnedArray<class ScriptRef> newScripts;
    juce::Array<juce::String> scriptNames;
    juce::OwnedArray<class BindingSet> newBindingSets;
    class ButtonSet* newButtons = nullptr;
    
    BasicLog log;

    BasicButtonRow commands;
    juce::TextButton loadCurrentButton {"Load Current"};
    juce::TextButton loadFileButton {"Load File"};
    juce::TextButton installButton {"Install"};
    juce::TextButton undoButton {"Undo"};
    
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFolder;

    void locateExisting();
    void doLoadCurrent();
    void doLoadFile();
    void doFileChooser();
    void doLoad(juce::File file);

    void loadUIConfig(juce::String xml);
    bool convertButton(juce::XmlElement* el);
    
    void loadMobiusConfig(juce::String xml);
    void loadPresets();
    void loadSetups();
    void loadScripts();
    juce::String verifyScript(class ScriptRef* ref);
    juce::String getScriptNameFromPath(const char* path);
    void registerDirectoryScripts(juce::File dir);
    juce::String getScriptName(juce::File file);
    
    void loadBindings();
    class BindingSet* loadBindings(class BindingSet* old, class BindingSet* master);
    class Binding* upgradeBinding(class Binding* src);
    int getLastDigit(juce::String s);
    bool addUpgradeButton(class DisplayButton* db);

    void doInstall();
    void noLoad();
    void mergeBindings(BindingSet* src, BindingSet* dest);
    
    void doUndo();
};

class UpgradePanel : public BasePanel
{
  public:

    UpgradePanel(class Supervisor* s) : content(s) {
        setTitle("Configuration File Upgrader");
        setContent(&content);
        setSize(800, 600);
    }
    ~UpgradePanel() {}

    void showing() override {
        content.showing();
    }
    
  private:

    UpgradeContent content;
};
