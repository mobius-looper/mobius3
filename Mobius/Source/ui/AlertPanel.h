/**
 * This one is not managed by PanelFactory because we have to pass an argument
 * to the show() method containing the text to show.
 *
 * It is the only "show with arguments" BasePanel we have right now, once we have
 * more than one consider generalizing this.  What might also be interesting is
 * letting these stack, allowing more than one concurrent AlertPanel with different
 * message, or keeping the same one but appending messages to it.
 *
 * This is also unusual because a show request can happen multiple times with
 * new messages to accumulate.  
 *
 */
#pragma once

#include <JuceHeader.h>
#include "BasePanel.h"

class AlertContent : public juce::Component
{
  public:

    static const int FontHeight = 20;
    static const int TextHeight = 100;

    AlertContent(class Supervisor* s);
    ~AlertContent() {}

    void setMessage(juce::String msg);
    void addMessage(juce::String msg);
    
    void resized() override;

  private:

    class Supervisor* supervisor = nullptr;

    juce::Label text;
    // juce::StringArray messages;

};    

class AlertPanel : public BasePanel
{
  public:

    AlertPanel(class Supervisor* s) : content(s) {
        // don't really need a title on these
        // but without a title bar you don't get mouse
        // events for dragging
        //setTitle("Alert");
        setContent(&content);
        // this gives it a yellow border
        setAlert();
        // this gives it dragability within the entire window since
        // these don't have a title bar
        followContentMouse();
        
        setSize(500, 200);
    }
    ~AlertPanel() {}

    void show(juce::String message);

  private:

    AlertContent content;
};

