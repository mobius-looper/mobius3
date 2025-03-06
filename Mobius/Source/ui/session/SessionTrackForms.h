/**
 * Extension of ParameterTreeForms that maintains the tree and forms
 * for one track.
 */

#pragma once

#include <Juceheader.h>

#include "../../model/Session.h"

#include "../parameter/DropTreeView.h"
#include "../parameter/ParameterForm.h"
#include "../parameter/ParameterTreeForms.h"
#include "../parameter/ParameterFormCollection.h"

class SessionTrackForms : public ParameterTreeForms,
                          public ParameterFormCollection::Factory,
                          // for drop onto the forms
                          public ParameterForm::Listener,
                          // for drop onto the tree
                          public DropTreeView::Listener,
                          // it is important that this be as high as possible to encompass
                          // all the drag sources
                          // !! would this be better on SessionTrackEditor
                          public juce::DragAndDropContainer
                         
{
  public:

    SessionTrackForms();
    ~SessionTrackForms();

    void initialize(class Provider* p, class Session::Track* def);
    void save();
    void cancel();
    void decacheForms();

    class ParameterForm* parameterFormCollectionCreate(juce::String formid) override;
    
    void parameterFormDrop(class ParameterForm* src, juce::String desc) override;

    void dropTreeViewDrop(DropTreeView* dtv, const juce::DragAndDropTarget::SourceDetails& details) override;

  private:
    
    class Provider* provider = nullptr;
    class Session::Track* sessionTrack = nullptr;
    ValueSet* values = nullptr;
};
