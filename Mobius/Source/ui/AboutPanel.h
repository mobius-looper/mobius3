
#pragma once

#include <JuceHeader.h>
#include "BasePanel.h"

class AboutContent : public juce::Component
{
  public:

    AboutContent();
    ~AboutContent() {}

    void resized() override;

  private:

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

    AboutPanel() {
        setTitle("About");
        setContent(&content);
        setSize(500, 200);
    }
    ~AboutPanel() {}

  private:

    AboutContent content;
};

