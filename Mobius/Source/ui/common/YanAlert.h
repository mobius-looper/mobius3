/**
 * Yet another popup alert dialog with a messsage and buttons.
 *
 * This wraps the juce::AlertWindow.
 * It works for basic messages with Ok/Cancel buttons, but it is really
 * obscure how to use it with custom components since the AlertWindow itself
 * does not have a showAsync method.  That is static and doesn't allow you
 * to directly create the AlertWindow, which you need in order to add custom
 * components.
 *
 * Whatever...see YanDialog for a plain old Component that behaves similar to
 * an async dialog window.
 *
 * In retrospct, full blown child windows have issues with plugins so this is
 * probably a bad idea anyway.  YanDialog is a normal component that can
 * also be used for alerts.
 */

#pragma once

#include <JuceHeader.h>

class YanAlert
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void yanAlertSelected(class YanAlert* d, int id) = 0;
    };

    YanAlert();
    YanAlert(Listener* l);
    ~YanAlert();

    void setListener(Listener* l);
    void setSerious(bool b);
    void setTitle(juce::String s);
    void setMessage(juce::String s);
    void addButton(juce::String s);

    void show();

  private:

    Listener* listener = nullptr;

    juce::String title;
    juce::String message;
    juce::StringArray buttons;
    bool serious = false;
    
    void showInternal();
    void showFinished(int button);

};


