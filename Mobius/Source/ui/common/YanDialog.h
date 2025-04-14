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

    static const int DefaultWidth = 400;
    static const int BorderWidth = 2;
    static const int MainInset = 2;

    static const int TitleDefaultHeight = 16;
    static const int TitlePostGap = 8;
    
    static const int ButtonTopGap = 8;
    static const int ButtonHeight = 20;
    static const int ButtonBottomGap = 4;

    static const int SectionGap = 8;
    static const int SectionDefaultTitleHeight 12;
    
    static const int ContentInset = 8;
    static const int ContentDefaultHeight = 200;
    
    static const int MessageDefaultHeight = 12;
    
    class TitleSection : public juce::Component {
      public:
        juce::String title;
        juce::Colour color;
        int height = TitleDefaultHeight;
    };
    
    class Message : {
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

    class MessageSection : public juce::Component {
      public:
        juce::String title;
        juce::Colour titleColor;
        int titleHeight = SectionDefaultTitleHeight;
        int messageHeight = MessageDefaultHeight;
        juce::Array<Message> messages;
    };

    class ContentSection : public juce::Component {
      public:
        // replaces the built-in form for complex content
        juce::Component* content = nullptr;
        
        // built-in form you can add fields to
        YanForm form;
    };
    
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

    void setListener(Listener* l);
    void setSerious(bool b);
    void setTitle(juce::String s);

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

    bool serious = false;
    juce::Colour borderColor;
    juce::Colour messageColor = juce::Colours::white;

    int titleGap = TitlePostGap;
    int sectionGap = SectionGap;
    int buttonGap = ButtonGap;

    TitleSection title;
    MessageSection messages;
    ContentSection content;
    MessageSection errors;
    MessageSection warnings;
    
    // dynamic button list
    juce::StringArray buttonNames;
    juce::OwnedArray<juce::Button> buttons;
    BasicButtonRow buttonRow;
    
    void init();
    
};


