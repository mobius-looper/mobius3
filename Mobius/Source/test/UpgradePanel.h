
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

    BasicLog log;

    UpgradePanelFooter footer;
    juce::TextButton okButton {"OK"};
    juce::TextButton loadButton {"Load File"};
    
    int centerLeft(juce::Component& c);
    int centerLeft(juce::Component* container, juce::Component& c);
    int centerTop(juce::Component* container, juce::Component& c);
    void centerInParent(juce::Component& c);
    void centerInParent();

    void doUpgrade();
    void doFileChooser();
    void doUpgrade(juce::File file);
    
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastFolder;

};

