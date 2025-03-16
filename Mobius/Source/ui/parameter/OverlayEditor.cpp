/**
 * ConfigEditor for the ParameterSets.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/ParameterSets.h"
#include "../../model/ValueSet.h"
#include "../../Supervisor.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "OverlayTable.h"
#include "OverlayTreeForms.h"

#include "OverlayEditor.h"

OverlayEditor::OverlayEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("OverlayEditor");

    table.reset(new OverlayTable(this));
    addAndMakeVisible(table.get());

    table->setListener(this);
}

OverlayEditor::~OverlayEditor()
{
}

void OverlayEditor::prepare()
{
}

void OverlayEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    table->setBounds(area.removeFromLeft(200));
    for (auto tf : treeForms)
      tf->setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditor overloads
//
//////////////////////////////////////////////////////////////////////

void OverlayEditor::load()
{
    ParameterSets* master = supervisor->getParameterSets();
    overlays.reset(new ParameterSets(master));
    revertOverlays.reset(new ParameterSets(master));
    treeForms.clear();

    table->load(overlays.get());

    for (auto set : overlays->getSets()) {
        OverlayTreeForms* otf = new OverlayTreeForms();
        otf->initialize(supervisor);
        otf->load(set);
        treeForms.add(otf);
        addChildComponent(otf);
    }

    table->selectFirst();
    resized();
}

void OverlayEditor::show(int index)
{
    if (index != currentSet) {
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
    }
}    

/**
 * Called by the Save button in the footer.
 *
 * Save is a little complicated and unlike how Sessions save.
 * Since we had a complete copy of the ParameterSets and don't need to deal with
 * outside modifications to portions of it, we can completely rebuild the ValueSet list
 * and put it in the master ParmaeterSets.
 */
void OverlayEditor::save()
{
    // save any forms that were built and displayed back to the ValueSets
    // in our copied ParameterSets
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

    // make sure dialogs are clean
    table->cancel();
}

/**
 * Throw away all editing state.
 */
void OverlayEditor::cancel()
{
    overlays.reset();
    revertOverlays.reset();
    table->clear();
    for (auto tf : treeForms)
      tf->cancel();
    table->cancel();
}

void OverlayEditor::decacheForms()
{
    // does this mske sense here?  they're dynamic so no
}

void OverlayEditor::revert()
{
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
void OverlayEditor::typicalTableChanged(TypicalTable* t, int row)
{
    (void)t;
    if (row < 0) {
        Trace(1, "SessionTrackEditor: Change alert with no selected track number");
    }
    else {
        show(row);
    }
}

bool OverlayEditor::checkName(juce::String newName, juce::StringArray& errors)
{
    bool ok = false;

    // todo: do some validation on the name
    bool validName = true;
    
    if (!validName) {
        errors.add(juce::String("Overlay name ") + newName + " contains illegal characters");
    }
    else {
        ValueSet* existing = overlays->find(newName);
        if (existing != nullptr) {
            errors.add(juce::String("Overlay name ") + newName + " is already in use");
        }
        else {
            ok = true;
        }
    }
    return ok;
}

void OverlayEditor::overlayTableNew(juce::String newName, juce::StringArray& errors)
{
    if (checkName(newName, errors)) {
        ValueSet* neu = new ValueSet();
        neu->name = newName;
        addNew(neu);
    }
}

void OverlayEditor::addNew(ValueSet* neu)
{
    overlays->add(neu);
    
    OverlayTreeForms* otf = new OverlayTreeForms();
    otf->initialize(supervisor);
    otf->load(neu);
    treeForms.add(otf);
    addChildComponent(otf);
    resized();

    table->reload();
    int newIndex = overlays->getSets().size() - 1;
    table->selectRow(newIndex);
    show(newIndex);
}

ValueSet* OverlayEditor::getSourceOverlay(juce::String action, juce::StringArray& errors)
{
    ValueSet* set = nullptr;
    if (currentSet < 0) {
        errors.add(juce::String("No overlay selected for ") + action);
    }
    else {
        set = overlays->getByIndex(currentSet);
        if (set == nullptr) {
            Trace(1, "OverlayEditor: Overlay ordinals are messed up");
            errors.add(juce::String("Internal error"));
        }
    }
    return set;
}

void OverlayEditor::overlayTableCopy(juce::String newName, juce::StringArray& errors)
{
    if (checkName(newName, errors)) {
        ValueSet* set = getSourceOverlay("Copy", errors);
        if (set != nullptr) {
            ValueSet* copy = new ValueSet(set);
            copy->name = newName;
            addNew(copy);
        }
    }
}

void OverlayEditor::overlayTableRename(juce::String newName, juce::StringArray& errors)
{
    if (checkName(newName, errors)) {
        ValueSet* set = getSourceOverlay("Rename", errors);
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
void OverlayEditor::overlayTableDelete(juce::StringArray& errors)
{
    ValueSet* set = getSourceOverlay("Delete", errors);
    if (set != nullptr) {
        if (!overlays->remove(set)) {
            Trace(1, "OverlayEditor: Problem removing overlay");
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
