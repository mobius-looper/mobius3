/**
 * ConfigEditor for the BindingSets.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/SystemConfig.h"
#include "../../model/BindingSets.h"
#include "../../model/BindingSet.h"
#include "../../model/Binding.h"
#include "../../Supervisor.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "BindingSetTable.h"
#include "BindingSetContent.h"

#include "BindingEditor.h"

BindingEditor::BindingEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("BindingEditor");

    setTable.reset(new BindingSetTable(this));
    addAndMakeVisible(setTable.get());

    setTable->setListener(this);

    addChildComponent(bindingDetails);
}

BindingEditor::~BindingEditor()
{
}

void BindingEditor::prepare()
{
}

void BindingEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    setTable->setBounds(area.removeFromLeft(200));
    for (auto c : contents)
      c->setBounds(area);
    
    //for (auto tf : treeForms)
    //tf->setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditor overloads
//
//////////////////////////////////////////////////////////////////////

void BindingEditor::load()
{
    SystemConfig* scon = supervisor->getSystemConfig();
    BindingSets* master = scon->getBindings();
    bindingSets.reset(new BindingSets(master));
    revertSets.reset(new BindingSets(master));
    //treeForms.clear();

    setTable->load(bindingSets.get());

    contents.clear();
    for (auto set : bindingSets->getSets()) {
        BindingSetContent* cont = new BindingSetContent();
        cont->load(this, set);
        contents.add(cont);
        addChildComponent(cont);
    }

    // reset this to trigger show() during selectFirst()
    currentSet = -1;

    setTable->selectFirst();

    bindingDetails.initialize(supervisor);
    
    resized();

    //JuceUtil::dumpComponent(this);
}

void BindingEditor::show(int index)
{
    if (index != currentSet) {

        if (currentSet >= 0) {
            BindingSetContent* existing =  contents[currentSet];
            existing->setVisible(false);
        }

        if (index >= 0 && index < contents.size()) {
            BindingSetContent* neu = contents[index];
            //cont->show();
            neu->setVisible(true);
            currentSet = index;
        }
        else {
            currentSet = -1;
        }
    }
}

void BindingEditor::showBinding(Binding* b)
{
    (void)b;

    bindingDetails.show(this);
    JuceUtil::centerInParent(&bindingDetails);
}

/**
 * Called by the Save button in the footer.
 */
void BindingEditor::save()
{
    // there are no unsaved forms in the BindingSetContent list atm

    // todo: magic stuff
    
    // make sure dialogs are clean
    setTable->cancel();
    for (auto cont : contents)
      cont->cancel();
}

/**
 * Throw away all editing state.
 */
void BindingEditor::cancel()
{
    bindingSets.reset();
    revertSets.reset();
    setTable->clear();
    for (auto cont : contents)
      cont->cancel();
    setTable->cancel();
}

void BindingEditor::decacheForms()
{
}

void BindingEditor::revert()
{
    cancel();
    
    // yeah, this isn't enough, need to do a full refresh like load() does
    // there is no revert button though so we're good for now
    bindingSets.reset(new BindingSets(revertSets.get()));
    
    // this doesn't actually work but you would refresh here
}

//////////////////////////////////////////////////////////////////////
//
// BindingSetTable Callbacks
//
//////////////////////////////////////////////////////////////////////

/**
 * This is called when the selected row changes either by clicking on
 * it or using the keyboard arrow keys after a row has been selected.
 */
void BindingEditor::typicalTableChanged(TypicalTable* t, int row)
{
    (void)t;
    if (row < 0) {
        Trace(1, "SessionTrackEditor: Change alert with no selected track number");
    }
    else {
        show(row);
    }
}

bool BindingEditor::checkName(juce::String newName, juce::StringArray& errors)
{
    bool ok = false;

    // todo: do some validation on the name
    bool validName = true;
    
    if (!validName) {
        errors.add(juce::String("Bindig Set name ") + newName + " contains illegal characters");
    }
    else {
        BindingSet* existing = bindingSets->find(newName);
        if (existing != nullptr) {
            errors.add(juce::String("Binding Set name ") + newName + " is already in use");
        }
        else {
            ok = true;
        }
    }
    return ok;
}

void BindingEditor::bindingSetTableNew(juce::String newName, juce::StringArray& errors)
{
    if (checkName(newName, errors)) {
        BindingSet* neu = new BindingSet();
        neu->name = newName;
        addNew(neu);
    }
}

void BindingEditor::addNew(BindingSet* neu)
{
    bindingSets->add(neu);

    BindingSetContent* cont = new BindingSetContent();
    cont->load(this, neu);
    contents.add(cont);
    addChildComponent(cont);
    resized();

    setTable->reload();
    int newIndex = bindingSets->getSets().size() - 1;
    setTable->selectRow(newIndex);
    show(newIndex);
}

BindingSet* BindingEditor::getSourceBindingSet(juce::String action, juce::StringArray& errors)
{
    BindingSet* set = nullptr;
    if (currentSet < 0) {
        errors.add(juce::String("No binding set selected for ") + action);
    }
    else {
        set = bindingSets->getByIndex(currentSet);
        if (set == nullptr) {
            Trace(1, "BindingEditor: BindingSet ordinals are messed up");
            errors.add(juce::String("Internal error"));
        }
    }
    return set;
}

void BindingEditor::bindingSetTableCopy(juce::String newName, juce::StringArray& errors)
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

void BindingEditor::bindingSetTableRename(juce::String newName, juce::StringArray& errors)
{
    if (checkName(newName, errors)) {
        BindingSet* set = getSourceBindingSet("Rename", errors);
        if (set != nullptr) {
            set->name = newName;
            setTable->reload();
        }
    }
}

/**
 * Deletion is complex since this overlay may be referenced in saved sessions
 * and we're not going to walk over all of them removing the reference.
 * Could at least make a stab at checking the loaded session though.
 * When a session with a state reference is loaded, it must adapt well.
 */
void BindingEditor::bindingSetTableDelete(juce::StringArray& errors)
{
    BindingSet* set = getSourceBindingSet("Delete", errors);
    if (set != nullptr) {
        if (!bindingSets->remove(set)) {
            Trace(1, "BindingEditor: Problem removing overlay");
            errors.add("Internal error");
        }
        else {
            if (currentSet >= 0) {
                BindingSetContent* cont = contents[currentSet];
                removeChildComponent(cont);
                contents.removeObject(cont, true);
            }
            
            // stay on the same table row with the ones
            // below shifted up, show() won't know about the OTF
            // we just deleted, to clear currentSet before calling it
            int newIndex = currentSet;
            if (newIndex >= bindingSets->getSets().size())
              newIndex = bindingSets->getSets().size() - 1;
            currentSet = -1;

            setTable->reload();
            setTable->selectRow(newIndex);
            show(newIndex);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
