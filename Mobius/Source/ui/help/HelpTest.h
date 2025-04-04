/**
 * A testing panel that shows the BarleyML demo
 */

#pragma once

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../BasePanel.h"
#include "../common/BasicButtonRow.h"
#include "../common/BasicLog.h"

#include "../../tools/BarelyML/BarelyML.h"
#include "../../tools/BarelyML/BarelyMLDemo.h"


class HelpContent : public juce::Component,
                     public juce::Button::Listener
{
  public:

    HelpContent(class Supervisor* s);
    ~HelpContent();

    void showing();
    void hiding();
    void update();

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Button::Listener
    void buttonClicked(juce::Button* b) override;
    
  private:

    class Supervisor* supervisor = nullptr;
    BasicButtonRow commandButtons;

    BarelyMLDemo demo;
    
    juce::TextButton clearButton {"Clear"};
    juce::TextButton refreshButton {"Refresh"};

};

class HelpPanel : public BasePanel
{
  public:

    HelpPanel(class Supervisor* s) : content(s) {
        setTitle("Help Demo");
        setContent(&content);
        setSize(400, 500);
    }
    ~HelpPanel() {}

    void update() override {
        content.update();
    }

    void showing() override {
        content.showing();
    }
    
    void hiding() override {
        content.hiding();
    }
    
  private:

    HelpContent content;
};
