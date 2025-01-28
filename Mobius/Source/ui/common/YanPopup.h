/**
 * Yet another popup menu.
 */

#pragma once

#include <JuceHeader.h>

class YanPopup
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void yanPopupSelected(YanPopup* pop, int id) = 0;
    };

    YanPopup();
    YanPopup(Listener* l);
    ~YanPopup();

    void setListener(Listener* l);
    void add(juce::String name, int id);
    void addDivider();

    void show();

  private:

    Listener* listener = nullptr;
       
    juce::PopupMenu menu;
    
    void popupSelection(int result);
};


