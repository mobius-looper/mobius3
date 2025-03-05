/**
 * A tree/form combo that edits the full set of deafult track parameters.
 */

#pragma once

#include <JuceHeader.h>

#include "ParameterForm.h"
#include "ParameterTreeForms.h"
#include "ParameterFormCollection.h"

class OverlayTreeForms : public ParameterTreeForms,
                         public ParameterFormCollection::Factory,
                         // for drop onto the forms
                         public ParameterForm::Listener,
                         // for drop onto the tree
                         public DropTreeView::Listener,
                         // it is important that this be as high as possible to encompass
                         // all the drag sources
                         public juce::DragAndDropContainer
                         
{
  public:

    OverlayTreeForms();
    ~OverlayTreeForms() {}

    // SessionEditor Interface

    void initialize(class Provider* p);
    
    void load(class ValueSet* set);
    void save(ValueSet* values);
    void cancel();
    void decacheForms();

    class ParameterForm* parameterFormCollectionCreate(juce::String formid) override;

    void parameterFormDrop(class ParameterForm* src, juce::String desc) override;

    void dropTreeViewDrop(DropTreeView* dtv, const juce::DragAndDropTarget::SourceDetails& details) override;
    
  private:

    class Provider* provider = nullptr;
    class ValueSet* values = nullptr;

};


