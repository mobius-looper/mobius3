/**
 * Subcomponent of ConfigPanel that provides widgetry to
 * select from a list of object names.
 */

#pragma once

#include <JuceHeader.h>

/**
 * The object selector presents a combobox to select one of a list
 * of objects.  It also displays the name of the selected object
 * for editing.   There is a set of buttons for acting on the object list.
 */
class ObjectSelector : public juce::Component,
                       public juce::Button::Listener,
                       public juce::ComboBox::Listener
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void objectSelectorSelect(int ordinal) = 0;
        virtual void objectSelectorRename(juce::String newName) = 0;
        virtual void objectSelectorNew(juce::String newName) = 0;
        virtual void objectSelectorDelete() = 0;
        virtual void objectSelectorCopy() = 0;
    };

    /**
     * The starting name to use for new objects.
     */
    const char* NewName = "[New]";
    
    ObjectSelector();
    ~ObjectSelector() override;
    
    void setListener(Listener* l) {
        listener = l;
    }

    void resized() override;
    void paint (juce::Graphics& g) override;

    int getPreferredHeight();

    // set the full list of names to display in the combo box
    void setObjectNames(juce::StringArray names);
    // add a name to the end
    void addObjectName(juce::String name);
    // change the selected name
    void setSelectedObject(int ordinal);
    // get the currently selected name
    juce::String getObjectName();
    // get the currently selected ordinal
    int getObjectOrdinal();
    
    // Button Listener
    void buttonClicked(juce::Button* b) override;

    // Combobox Listener
    void comboBoxChanged(juce::ComboBox* combo) override;
  
  private:

    //class ConfigPanel* parentPanel;
    Listener* listener = nullptr;
    juce::ComboBox combobox;
    int lastId = 0;
    
    juce::TextButton newButton {"New"};
    juce::TextButton deleteButton {"Delete"};
    juce::TextButton copyButton {"Copy"};

    // decided to put this in the footer instead
    //juce::TextButton revertButton {"Revert"};
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
