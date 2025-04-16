
#pragma once

#include <JuceHeader.h>

#include "YanField.h"

class YanListBox : public YanField, public juce::ListBoxModel
{
  public:

    YanListBox(juce::String label);
    ~YanListBox();

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void yanListBoxSelected(YanListBox* c, int selection) = 0;
    };
    void setListener(Listener* l);

    void setItems(juce::StringArray names);
    void setSelection(int index);
    int getSelection();

    int getPreferredComponentWidth() override;
    int getPreferredComponentHeight() override;
    
    void resized() override;

    // ListBoxModel
    void paintListBoxItem (int rowNumber, juce::Graphics& g,
                           int width, int height, bool rowIsSelected) override;
    int getNumRows() override;
    juce::String getNameForRow(int row) override;
    
  private:

    Listener* listener = nullptr;

    juce::StringArray items;
    juce::ListBox listbox;
};


