/**
 * A table showing a list of Bindings being edited.
 *
 * Where should cell formatting go, in the paretent or in the Binding?
 * Having it in binding makes the interface here simpler, but adds
 * a lot of display stuff to Binding which is a simple model.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../common/ButtonBar.h"

class OldBindingTable : public juce::Component, public juce::TableListBoxModel, public ButtonBar::Listener
{
  public:

    // column ids 
    static const int TargetColumn = 1;
    static const int TriggerColumn = 2;
    static const int ArgumentsColumn = 3;
    static const int ScopeColumn = 4;
    static const int DisplayNameColumn = 5;
    
    class Listener {
      public:
        virtual ~Listener() {}
        virtual juce::String renderTriggerCell(class Binding* b) = 0;
        virtual void bindingSelected(class Binding* b) = 0;
        virtual void bindingDeselected() = 0;
        virtual void bindingUpdate(class Binding* b) = 0;
        virtual void bindingDelete(class Binding* b) = 0;
        virtual class Binding* bindingNew() = 0;
        virtual class Binding* bindingCopy(class Binding* b) = 0;
    };

    OldBindingTable();
    ~OldBindingTable();

    //
    // Constructor options
    //
    
    // option to hide the trigger column
    void removeTrigger();
    // option to add the display name column
    void addDisplayName();
    
    void setListener(Listener* l) {
        listener = l;
    }

    void setOrdered(bool b);

    void add(class Binding* binding);
    void clear();
    void updateContent();
    void deselect();

    // return the edited list, ownership transfers to the caller
    void captureBindings(juce::Array<Binding*>& dest);

    // get the Binding that is currently selected
    class Binding* getSelectedBinding();

    int getPreferredWidth();
    int getPreferredHeight();

    // return true if this was creatred as a placeholder row for a new binding
    bool isNew(Binding* b);

    // ButtonBar::Listener
    void buttonClicked(juce::String name) override;

    // Component
    void resized() override;

    // TableListBoxModel
    
    int getNumRows() override;
    void paintRowBackground (juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected) override;
    void paintCell (juce::Graphics& g, int rowNumber, int columnId,
                    int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

  private:

    juce::OwnedArray<class Binding> bindings;
    Listener* listener = nullptr;
    bool ordered = false;
    int targetColumn = 0;
    int triggerColumn = 0;
    int argumentsColumn = 0;
    int scopeColumn = 0;
    int displayNameColumn = 0;
    
    ButtonBar commands;
    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};
    int lastSelection = -1;
    
    void initTable();
    void initColumns();

    juce::String getCellText(int row, int columnId);
    juce::String formatTriggerText(class Binding* b);
    juce::String formatScopeText(class Binding* b);
    
};
    
