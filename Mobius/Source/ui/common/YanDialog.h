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

    class Message {
      public:
        Message() {}
        Message(juce::String s) {message = s;}
        juce::String prefix;
        juce::Colour prefixColor;
        int prefixHeight = 0;
        juce::String message;
        juce::Colour messageColor;
        int messageHeight = 0;
    };

    class Section {
      public:
        juce::String title;
        juce::Colour titleColor;
        int titleHeight = 0;
        juce::Array<Message> messages;
    };
    
    static const int DefaultWidth = 400;
    static const int DefaultContentHeight = 200;
    static const int BorderWidth = 2;
    static const int MainInset = 2;
    
    static const int TitleHeight = 16;
    static const int TitlePostGap = 8;
    static const int MessageFontHeight = 12;
    static const int MessagePostGap = 4;
    static const int ContentInset = 8;
    static const int ButtonGap = 8;
    static const int BottomGap = 4;
    static const int ButtonHeight = 20;
    
    class Listener {
      public:
        virtual ~Listener() {}
        virtual void yanDialogClosed(YanDialog* d, int buttonIndex) = 0;
    };

    YanDialog();
    YanDialog(Listener* l);
    ~YanDialog();

    void setId(int id);
    int getId();

    void setWidth(int w);
    void setHeight(int h);
    void setTitleHeight(int h);
    void setTitleColor(juce::Colour c);
    void setTitleGap(int h);
    void setMessageHeight(int h);
    void setMessageColor(juce::Colour c);
    void setWarningColor(juce::Colour c);
    void setErrorColor(juce::Colour c);
    void setWarningHeight(int h);
    void setErrorHeight(int h);
    void setSectionHeight(int h);
    void setButtonGap(int h);
    
    void setListener(Listener* l);
    void setSerious(bool b);
    void setTitle(juce::String s);
    
    void clearMessages();
    void addMessage(juce::String s);
    void addMessage(Message m);
    void addMessageGap(int h = 0);
    void setMessage(juce::String s);
    void addWarning(juce::String s);
    void addError(juce::String s);
    void clearButtons();
    void addButton(juce::String text);
    void setButtons(juce::String csv);
    
    void addField(class YanField* f);
    void setContent(juce::Component* c);

    void show();
    void show(juce::Component* parent);
    void cancel();
    void clear();
    void reset();
    
    void resized();
    void paint(juce::Graphics& g);
    void buttonClicked(juce::Button* b);

  private:

    Listener* listener = nullptr;
    int id = 0;
    int requestedWidth = 0;
    int requestedHeight = 0;
    int titleHeight = TitleHeight;
    int titleGap = TitlePostGap;
    int sectionGap = TitlePostGap;
    int messageHeight = MessageFontHeight;
    int warningHeight = MessageFontHeight;
    int errorHeight = MessageFontHeight;
    int sectionHeight = MessageFontHeight;
    int buttonGap = ButtonGap;

    juce::Colour titleColor = juce::Colours::green;
    juce::Colour messageColor = juce::Colours::white;
    juce::Colour warningColor = juce::Colours::white;
    juce::Colour errorColor = juce::Colours::white;
    
    bool serious = false;
    juce::Colour borderColor;
    juce::String title;
    juce::Array<Message> messages;
    juce::Array<Message> warnings;
    juce::Array<Message> errors;
    
    // built-in form you can add fields to
    YanForm form;
    
    // replaces the built-in form for complex content
    juce::Component* content = nullptr;
    
    // dynamic button list
    juce::StringArray buttonNames;
    juce::OwnedArray<juce::Button> buttons;
    BasicButtonRow buttonRow;
    
    void init();
    //void layoutButtons(juce::Rectangle<int> area);
    int getMessagesHeight();
    int getWarningsHeight();
    int getMessageHeight(Message& m, int defaultHeight);
    int getErrorsHeight();
    int getMessagesHeight(juce::Array<Message>& list, int mheight);
    int getDefaultHeight();

    void renderMessage(juce::Graphics& g, Message& m, juce::Rectangle<int>& area, int top, int height, juce::Colour defaultColor);
};


