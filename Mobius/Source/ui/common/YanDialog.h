/**
 * Dialog popups display whatever you put into them and have configurable
 * buttons at the bottom.  If you ask for nothing else they have a single
 * Ok button.
 *
 * Unlike BasePanels they have no title bar and don't need to be wired in to PanelFactory.
 * You* can add them as a child of any other component and toggle visibility.
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
#include "YanForm.h"

class YanDialog : public juce::Component, public juce::Button::Listener
{
  public:

    static const int DefaultWidth = 300;
    static const int DefaultHeight = 200;

    static const int BorderWidth = 2;
    static const int TitleInset = 6;
    static const int TitleHeight = 20;
    static const int TitleMessageGap = 10;
    static const int MessageHeight = 20;
    static const int MessageFontHeight = 14;
    static const int ContentInset = 8;

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void yanDialogClosed(YanDialog* d, int button) = 0;
    };

    YanDialog();
    YanDialog(Listener* l);
    ~YanDialog();

    void setListener(Listener* l);
    void setSerious(bool b);
    void setTitle(juce::String s);
    void setMessage(juce::String s);
    void setMessageHeight(int h);
    void addButton(juce::String text);
    void addField(class YanField* f);
    void setContent(juce::Component* c);

    void show();
    void show(juce::Component* parent);

    void resized();
    void paint(juce::Graphics& g);
    void buttonClicked(juce::Button* b);

  private:

    Listener* listener = nullptr;

    bool serious = false;
    juce::Colour borderColor;
    juce::String title;
    juce::String message;
    int messageHeight = 0;
    
    // built-in form you can add fields to
    YanForm form;

    // replaces the built-in form for complex content
    juce::Component* content = nullptr;
    
    // dynamic button list
    juce::OwnedArray<juce::TextButton> buttons;
    BasicButtonRow buttonRow;

    juce::TextButton okButton {"Ok"};
    juce::TextButton cancelButton {"Cancel"};
    
    void init();
    //void layoutButtons(juce::Rectangle<int> area);
    int getMessageHeight();
};


