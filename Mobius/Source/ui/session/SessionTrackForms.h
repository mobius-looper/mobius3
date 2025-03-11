/**
 * Extension of ParameterTreeForms that maintains the tree and forms
 * for one track.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Session.h"

#include "../common/YanParameter.h"
#include "../parameter/DropTreeView.h"
#include "../parameter/ParameterForm.h"
#include "../parameter/ParameterTreeForms.h"
#include "../parameter/ParameterFormCollection.h"
#include "SessionOcclusions.h"

class SessionTrackForms : public ParameterTreeForms,
                          public ParameterFormCollection::Factory,
                          // for smarter field refresh
                          public ParameterForm::Refresher,
                          // for drop onto the forms
                          public ParameterForm::Listener,
                          // for drop onto the tree
                          public DropTreeView::Listener,
                          // for changes to the track overlay combo boxes
                          public YanParameter::Listener,
                          // it is important that this be as high as possible to encompass
                          // all the drag sources
                          // !! would this be better on SessionTrackEditor
                          public juce::DragAndDropContainer
                         
{
  public:

    SessionTrackForms();
    ~SessionTrackForms();

    void initialize(class Provider* p, class SessionEditor* se,
                    class Session* s, class Session::Track* def);
    void reload();
    void save();
    void cancel();
    void decacheForms();
    void sessionOverlayChanged();

    class ParameterForm* parameterFormCollectionCreate(juce::String formid) override;
    
    void parameterFormDrop(class ParameterForm* src, juce::String desc) override;
    void parameterFormClick(class ParameterForm* src, class YanParameter* p, const juce::MouseEvent& e) override;
    void parameterFormRefresh(class ParameterForm* src, class YanParameter* p) override;
    
    void dropTreeViewDrop(DropTreeView* dtv, const juce::DragAndDropTarget::SourceDetails& details) override;

    void yanParameterChanged(YanParameter* p) override;
    
  private:
    
    class Provider* provider = nullptr;
    class SessionEditor* editor = nullptr;
    class Session* session = nullptr;
    class Session::Track* sessionTrack = nullptr;
    ValueSet* values = nullptr;
    SessionOcclusions occlusions;

    // this determines the style of form population
    // when true, the form will be fully populated with unlockable fields
    // showing the default values
    // when false, forms show only those fields that are being overridden and
    // override fields must be dragged in
    bool lockingStyle = true;
    
    void toggleParameterDefault(class YanParameter* p);
    void refreshOcclusionList();
};
