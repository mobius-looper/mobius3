/**
 * ConfigEditor for the ParameterSets.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/ParameterSets.h"
#include "../../model/BindingSet.h"
#include "../../Supervisor.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "OverlayTable.h"
#include "OverlayTreeForms.h"

#include "BindingEditor.cpp.h"

BindingEditor.cpp::BindingEditor.cpp(Supervisor* s) : ConfigEditor(s)
{
    setName("BindingEditor.cpp");

    table.reset(new OverlayTable(this));
    addAndMakeVisible(table.get());

    table->setListener(this);
}

BindingEditor.cpp::~BindingEditor.cpp()
{
}

void BindingEditor.cpp::prepare()
{
}

void BindingEditor.cpp::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    table->setBounds(area.removeFromLeft(200));
    
    //for (auto tf : treeForms)
    //tf->setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditor overloads
//
//////////////////////////////////////////////////////////////////////

void BindingEditor.cpp::load()
{
    SystemConfig* scon = supervisor->getSystemConfig();
    BindingSets* master = scon->getBindings();
    bindingSets.reset(new BindingSets(master));
    revertOverlays.reset(new BindingSets(master));
    //treeForms.clear();

    table->load(bindingSets.get());

    #if 0
    for (auto set : bindingSets->getSets()) {
        OverlayTreeForms* otf = new OverlayTreeForms();
        otf->initialize(supervisor);
        otf->load(set);
        treeForms.add(otf);
        addChildComponent(otf);
    }
    #endif

    // reset this to trigger show() during selectFirst()
    currentSet = -1;

    table->selectFirst();
    resized();
}

void BindingEditor.cpp::show(int index)
{
    if (index != currentSet) {
#if 0        
        if (currentSet >= 0) {
            OverlayTreeForms* existing = treeForms[currentSet];
            existing->setVisible(false);
        }

        if (index >= 0 && index < treeForms.size()) {
            OverlayTreeForms* neu = treeForms[index];
            neu->show();
            neu->setVisible(true);
            currentSet = index;
        }
        else {
            currentSet = -1;
        }
#endif        
    }
}    

/**
 * Called by the Save button in the footer.
 */
void BindingEditor.cpp::save()
{
    // save any forms that were built and displayed back to the ValueSets
    // in our copied ParameterSets
#if 0    
    for (auto tf : treeForms) {
        // DynamicTreeForms saved back ot the ValueSet it was created with
        // OverlayTreeForms wants a target, pass null to behave like DTFs
        //tf->save();
        tf->save(nullptr);
    }

    // rebuild the list for the master ParameterSets container
    ParameterSets* master = supervisor->getParameterSets();
    master->transfer(overlays.get());

    supervisor->updateParameterSets();
#endif
    
    // make sure dialogs are clean
    table->cancel();
}

/**
 * Throw away all editing state.
 */
void BindingEditor.cpp::cancel()
{
    overlays.reset();
    revertOverlays.reset();
    table->clear();
    //for (auto tf : treeForms)
    //tf->cancel();
    table->cancel();
}

void BindingEditor.cpp::decacheForms()
{
    // does this mske sense here?  they're dynamic so no
}

void BindingEditor.cpp::revert()
{
    // yeah, this isn't enough, need to do a full refresh like load() does
    // there is no revert button though so we're good for now
    overlays.reset(new ParameterSets(revertOverlays.get()));
}

//////////////////////////////////////////////////////////////////////
//
// OverlayTable Callbacks
//
//////////////////////////////////////////////////////////////////////

/**
 * This is called when the selected row changes either by clicking on
 * it or using the keyboard arrow keys after a row has been selected.
 */
void BindingEditor.cpp::typicalTableChanged(TypicalTable* t, int row)
{
    (void)t;
    if (row < 0) {
        Trace(1, "SessionTrackEditor: Change alert with no selected track number");
    }
    else {
        show(row);
    }
}

bool BindingEditor.cpp::checkName(juce::String newName, juce::StringArray& errors)
{
    bool ok = false;

    // todo: do some validation on the name
    bool validName = true;
    
    if (!validName) {
        errors.add(juce::String("Overlay name ") + newName + " contains illegal characters");
    }
    else {
        BindingSet* existing = bindingSets->find(newName);
        if (existing != nullptr) {
            errors.add(juce::String("Overlay name ") + newName + " is already in use");
        }
        else {
            ok = true;
        }
    }
    return ok;
}

void BindingEditor.cpp::bindingSetTableNew(juce::String newName, juce::StringArray& errors)
{
    if (checkName(newName, errors)) {
        BindingSet* neu = new BindingSet();
        neu->name = newName;
        addNew(neu);
    }
}

void BindingEditor.cpp::addNew(BindingSet* neu)
{
    bindingSets->add(neu);

#if 0    
    OverlayTreeForms* otf = new OverlayTreeForms();
    otf->initialize(supervisor);
    otf->load(neu);
    treeForms.add(otf);
    addChildComponent(otf);
#endif    
    resized();

    table->reload();
    int newIndex = bindingSets->getSets().size() - 1;
    table->selectRow(newIndex);
    show(newIndex);
}

BindingSet* BindingEditor.cpp::getSourceBindingSet(juce::String action, juce::StringArray& errors)
{
    BindingSet* set = nullptr;
    if (currentSet < 0) {
        errors.add(juce::String("No binding set selected for ") + action);
    }
    else {
        set = bindingSets->getByIndex(currentSet);
        if (set == nullptr) {
            Trace(1, "BindingEditor.cpp: BindingSet ordinals are messed up");
            errors.add(juce::String("Internal error"));
        }
    }
    return set;
}

void BindingEditor.cpp::bindingSetTableCopy(juce::String newName, juce::StringArray& errors)
{
    if (checkName(newName, errors)) {
        BindingSet* set = getSourceBindingSet("Copy", errors);
        if (set != nullptr) {
            BindingSet* copy = new BindingSet(set);
            copy->name = newName;
            addNew(copy);
        }
    }
}

void BindingEditor.cpp::bindingSetTableRename(juce::String newName, juce::StringArray& errors)
{
    if (checkName(newName, errors)) {
        BindingSet* set = getSourceBindingSet("Rename", errors);
        if (set != nullptr) {
            set->name = newName;
            table->reload();
        }
    }
}

/**
 * Deletion is complex since this overlay may be referenced in saved sessions
 * and we're not going to walk over all of them removing the reference.
 * Could at least make a stab at checking the loaded session though.
 * When a session with a state reference is loaded, it must adapt well.
 */
void BindingEditor.cpp::bindingSetTableDelete(juce::StringArray& errors)
{
    BindingSet* set = getSourceBindingSet("Delete", errors);
    if (set != nullptr) {
        if (!bindingSets->remove(set)) {
            Trace(1, "BindingEditor.cpp: Problem removing overlay");
            errors.add("Internal error");
        }
        else {
            OverlayTreeForms* otf = treeForms[currentSet];
            removeChildComponent(otf);
            treeForms.removeObject(otf, true);

            // stay on the same table row with the ones
            // below shifted up, show() won't know about the OTF
            // we just deleted, to clear currentSet before calling it
            int newIndex = currentSet;
            if (newIndex >= treeForms.size())
              newIndex = treeForms.size() - 1;
            currentSet = -1;

            table->reload();
            table->selectRow(newIndex);
            show(newIndex);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
