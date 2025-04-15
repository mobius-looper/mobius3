/**
 * Yet another stab at configurable dialog popup components.
 *
 * Unlike BasePanels they have no title bar and don't need to be wired in to PanelFactory.
 * You add them as a child of any other component and toggle visibility.
 *
 * Dialog anatomy consists of these things, all optional:
 *
 *    Title - displayed at the top
 *    Messages - random text messages containing information
 *    Content - either a random component or a YanForm
 *    Errors - a section containing error messages
 *    Warnings - a section containing warning messages
 *    Buttons - configurable close buttons, must be at least one
 *
 */

#pragma once

#include <JuceHeader.h>

// make a Yan version of this
#include "BasicButtonRow.h"

#include "YanForm.h"

class YanDialog : public juce::Component, public juce::Button::Listener
{
  public:

    // various default sizes, msot can be overridden

    static const int DefaultWidth = 400;
    static const int BorderWidth = 2;
    static const int MainInset = 2;

    static const int TitleDefaultHeight = 24;
    static const int TitlePostGap = 12;
    
    static const int SectionTitleDefaultHeight = 20;
    static const int SectionPostGap = 8;
    
    static const int ContentInset = 8;
    static const int ContentDefaultHeight = 200;
    
    static const int MessageDefaultHeight = 20;
    static const int MessageTitlePostGap = 4;
    
    static const int ButtonTopGap = 8;
    static const int ButtonHeight = 20;
    static const int ButtonBottomGap = 4;

    class TitleSection : public juce::Component {
      public:
        juce::Colour borderColor;
        juce::Colour backgroundColor = juce::Colours::darkgrey;
        juce::String title;
        juce::Colour color = juce::Colours::white;
        int height = TitleDefaultHeight;
        int postGap = TitlePostGap;
        void clear();
        int getPreferredHeight();
        void paint(juce::Graphics& g) override;
    };
    
    class Message {
      public:
        Message() {}
        Message(juce::String s) {message = s;}
        juce::String prefix;
        juce::Colour prefixColor = juce::Colours::blue;
        int prefixHeight = 0;
        juce::String message;
        juce::Colour messageColor = juce::Colours::white;
        int messageHeight = 0;
    };

    class MessageSection : public juce::Component {
      public:
        juce::Colour borderColor;
        juce::String title;
        juce::Colour titleColor = juce::Colours::white;
        int titleHeight = SectionTitleDefaultHeight;
        int titlePostGap = MessageTitlePostGap;
        int messageHeight = MessageDefaultHeight;
        int postGap = SectionPostGap;

        void add(Message& m);
        void clear();
        int getPreferredHeight();
        void paint(juce::Graphics& g) override;
      private:
        int getMessageHeight(Message& m);
        juce::Array<Message> messages;
    };

    class ContentSection : public juce::Component {
      public:

        juce::Colour borderColor;
        int postGap = SectionPostGap;
        
        void addField(YanField* f);
        void setContent(juce::Component* c);
        void clear();
        int getPreferredHeight();
        void resized() override;
        void paint(juce::Graphics& g) override;
      private:
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

    // width is usually specified, height will usually be calculated
    void setWidth(int w);
    void setHeight(int h);

    void setBorderColor(juce::Colour c);
    
    void setTitle(juce::String s);
    void setTitleHeight(int h);
    void setTitleColor(juce::Colour c);
    void setTitleGap(int h);
    
    void setMessageHeight(int h);
    void setMessageColor(juce::Colour c);
    void setWarningColor(juce::Colour c);
    void setErrorColor(juce::Colour c);
    void setWarningHeight(int h);
    void setErrorHeight(int h);
    void setSectionTitleHeight(int h);
    void setButtonGap(int h);
    void setSectionGap(int h);

    // specification of the subcomponents
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

    void setTestBorders(juce::Colour c);

  private:

    Listener* listener = nullptr;
    int id = 0;
    int requestedWidth = 0;
    int requestedHeight = 0;
    juce::Colour borderColor;

    int buttonGap = ButtonTopGap;

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
    int getPreferredHeight();
    
};


