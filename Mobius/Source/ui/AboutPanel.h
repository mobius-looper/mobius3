
#pragma once

class AboutFooter : public juce::Component {
  public: 
    AboutFooter() {}
    ~AboutFooter() {}
};

class AboutPanel : public juce::Component, juce::Button::Listener
{
  public:

    AboutPanel();
    ~AboutPanel();

    void show();

    void resized() override;
    void paint(juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;

  private:

    juce::Label build;
    juce::Label root;
    juce::URL url;
    juce::HyperlinkButton hyper;

    AboutFooter footer;
    juce::TextButton okButton {"OK"};

    int centerLeft(juce::Component& c);
    int centerLeft(juce::Component* container, juce::Component& c);
    int centerTop(juce::Component* container, juce::Component& c);
    void centerInParent(juce::Component& c);
    void centerInParent();
};

