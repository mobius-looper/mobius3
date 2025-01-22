/**
 * Yet another popup alert dialog with a messsage and buttons.
 */

#pragma once

#include <JuceHeader.h>

class YanDialog
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void yanDialogSelected(class YanDialog* d, int id) = 0;
    };

    YanDialog();
    YanDialog(Listener* l);
    ~YanDialog();

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


