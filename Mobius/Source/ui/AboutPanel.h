
#pragma once

#include <JuceHeader.h>
#include "BasePanel.h"

class AboutContent : public juce::Component
{
  public:

    AboutContent(class Supervisor* s);
    ~AboutContent() {}

    void resized() override;

  private:
    class Supervisor* supervisor = nullptr;
    juce::Label product;
    juce::Label copyright;
    juce::URL url;
    juce::HyperlinkButton hyper;
    juce::Label build;
    juce::Label root;

};    

class AboutPanel : public BasePanel
{
  public:

    AboutPanel(class Supervisor* s) : content(s) {
        setTitle("About");
        setContent(&content);
        setSize(500, 200);
    }
    ~AboutPanel() {}

  private:
    AboutContent content;
};

