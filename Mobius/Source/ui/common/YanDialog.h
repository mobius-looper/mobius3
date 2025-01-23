/**
 * Dialog popups display whatever you put into them and have configurable
 * buttons at the bottom.  If you ask for nothing else they have a single
 * Ok button.
 *
 * The Listener is called when a button is clicked.  Unlike BasePanels they
 * have no title bar and don't need to be wired in to PanelFactory.  You
 * can add them as a child of any other component and toggle visibility.
 *
 * For non-interactive "alert" dialogs they can be given a title and a message
 * and will organize internal components around those.  They can be given a single
 * custom component and give it the full area on resize, minus the title and message.
 *
 * Need a YanButtonRow...
 */

#pragma once

#include <JuceHeader.h>

// make a Yan version of this
#include "BasicButtonRow.h"

class YanDialog : public juce::Component, public juce::Button::Listener
{
  public:

    static const int DefaultWidth = 300;
    static const int DefaultHeight = 200;

    static const int BorderWidth = 2;
    static const int TitleInset = 12;
    static const int TitleHeight = 20;
    static const int MessageHeight = 20;
    static const int ContentInset = 20;

    typedef enum {
        ButtonsOk,
        ButtonsOkCancel
    } ButtonStyle;

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void yanDialogOk(YanDialog* d) = 0;
    };

    YanDialog();
    YanDialog(Listener* l);
    ~YanDialog();

    void setListener(Listener* l);
    void setTitle(juce::String s);
    void setMessage(juce::String s);
    void setContent(juce::Component* c);
    void setButtonStyle(ButtonStyle s);

    void show();

    void resized();
    void paint(juce::Graphics& g);
    void buttonClicked(juce::Button* b);

  private:

    Listener* listener = nullptr;

    juce::Colour borderColor;
    juce::String title;
    juce::String message;
    juce::Component* content = nullptr;
    ButtonStyle buttonStyle = ButtonsOk;

    juce::TextButton okButton {"Ok"};
    juce::TextButton cancelButton {"Cancel"};
    BasicButtonRow buttons;
    
    void init();
    //void layoutButtons(juce::Rectangle<int> area);

};


