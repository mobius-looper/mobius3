/**
 * Base class for binding editing panels.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/Field.h"

#include "ConfigPanel.h"
#include "BindingTable.h"
#include "BindingTargetPanel.h"

class BindingPanel : public ConfigPanel, public BindingTable::Listener, public Field::Listener
{
  public:

    void traceBindingList(const char* title, Binding* blist);
    void traceBindingList(const char* title, juce::Array<Binding*> &blist);

    // hmm, tried to make these pure virtual but got an abort when called
    // something about base classes calling down to their subclass, wtf?
    virtual juce::String renderSubclassTrigger(class Binding* b) = 0;
    virtual bool isRelevant(class Binding* b) = 0;
    virtual void addSubclassFields() = 0;
    virtual void refreshSubclassFields(class Binding* b) = 0;
    virtual void captureSubclassFields(class Binding* b) = 0;
    virtual void resetSubclassFields() = 0;

    BindingPanel(class ConfigEditor *, const char* title, bool multi);
    virtual ~BindingPanel();

    // ConfigPanel overloads
    virtual void load();
    virtual void save();
    virtual void cancel();

    void resized();

    // BindingTable
    juce::String renderTriggerCell(Binding* b);
    void bindingSelected(class Binding* b);
    void bindingUpdate(class Binding* b);
    void bindingDelete(class Binding* b);
    class Binding* bindingNew();

    // Field
    void fieldChanged(Field* field);
    
  protected:

    BindingTable bindings;
    BindingTargetPanel targets;
    Form form;
    Field* scope = nullptr;
    Field* arguments = nullptr;
    int maxTracks = 0;
    int maxGroups = 0;

    void initForm();
    void resetForm();
    
  private:

    void render();
    void rebuildTable();

    void refreshForm(Binding* b);
    void captureForm(Binding* b);

    // start making this more like Preset and other multi-object panels
    juce::OwnedArray<BindingSet> bindingSets;
    juce::OwnedArray<BindingSet> revertBindingSets;
    int selectedBindingSet = 0;

    void loadNew();
    void loadBindingSet(int ordinal);
    void saveNew();
    void cancelNew();
    void saveBindingSet(int index);
    void saveBindingSet(BindingSet* dest);


};
