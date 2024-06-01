/**
 * An extension of Panel that keeps track of the FieldGrids within it.
 * Form panels may have a name which will become the tab name in tabbed forms.
 * The panel owns the FieldGrids and will delete them.
 *
 * In theory other Components could be added to this panel but that doesn't
 * happen yet.
 */

#pragma once

#include <JuceHeader.h>

#include "Panel.h"
#include "FieldGrid.h"

class FormPanel : public Panel
{
  public:
    
    FormPanel();
    FormPanel(juce::String /* tabName */);

    juce::String getTabName() {
        return tabName;
    }

    void addHeader(Component* c);
    void addFooter(Component* c);
    
    void addGrid(FieldGrid* grid);
    FieldGrid* getGrid(int index);
    void gatherFields(juce::Array<Field*>& fields);

    void render();
    juce::Rectangle<int> getMinimumSize();
    
    void resized() override;

  private:

    int layoutComponents(juce::OwnedArray<Component>* stuff, int rowOffset);

    juce::String tabName;
    juce::OwnedArray<juce::Component> header;
    juce::OwnedArray<FieldGrid> grids;
    juce::OwnedArray<juce::Component> footer;

};

