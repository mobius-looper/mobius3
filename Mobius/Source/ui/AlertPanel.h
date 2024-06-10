
#pragma once

#include <JuceHeader.h>
#include "BasePanel.h"

class AlertContent : public juce::Component
{
  public:

    static const int FontHeight = 20;
    static const int TextHeight = 100;

    AlertContent();
    ~AlertContent() {}

    void setMessage(juce::String msg) {
        text.setText(msg, juce::NotificationType::dontSendNotification);
    }
    
    void resized() override;

  private:

    juce::Label text;

};    

class AlertPanel : public BasePanel
{
  public:

    AlertPanel() {
        // don't really need a title on these
        // but without a title bar you don't get mouse
        // events for dragging
        //setTitle("Alert");
        setContent(&content);
        setAlert();
        followContentMouse();
        
        setSize(500, 200);
    }
    ~AlertPanel() {}

    void show(juce::String message);

  private:

    AlertContent content;
};

