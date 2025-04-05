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
#include "BindingTree.h"
#include "BindingSetContent.h"

#include "BindingEditor.h"

BindingEditor::BindingEditor(Supervisor* s, bool argButtons) : ConfigEditor(s)
{
    setName("BindingEditor");
    buttons = argButtons;

    setTable.reset(new BindingSetTable(this));
    addAndMakeVisible(setTable.get());
    setTable->setListener(this);

    bindingTree.reset(new BindingTree());
    bindingTree->setDraggable(true);
    bindingTree->setListener(this);
    addAndMakeVisible(bindingTree.get());

    addChildComponent(bindingDetails);
}

BindingEditor::~BindingEditor()
{
}

Provider* BindingEditor::getProvider()
{
    return supervisor;
}

void BindingEditor::prepare()
{
    bindingTree->initialize(supervisor);
}

void BindingEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    setTable->setBounds(area.removeFromLeft(200));

    bindingTree->setBounds(area.removeFromLeft(200));
    
    for (auto c : contents)
      c->setBounds(area);
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
    BindingSets* copy = new BindingSets(master);
    install(copy);
}

void BindingEditor::install(BindingSets* sets)
{
    bindingSets.reset(sets);

    setTable->load(bindingSets.get());

    contents.clear();
    for (auto set : bindingSets->getSets()) {
        install(set);
    }

    // reset this to trigger show() during selectFirst()
    currentSet = -1;

    setTable->selectFirst();

    bindingDetails.initialize(supervisor);
    
    resized();

    //JuceUtil::dumpComponent(this);
}

void BindingEditor::install(BindingSet* set)
{
    BindingSetContent* cont = new BindingSetContent();
    cont->initialize(isButtons());
    cont->load(this, set);
    contents.add(cont);
    addChildComponent(cont);
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

    bindingDetails.setCapturing(capturing);
    bindingDetails.show(this, this, b);
    JuceUtil::centerInParent(&bindingDetails);
}

void BindingEditor::bindingSaved()
{
    // jesus fuck, the chain of command here is messy
    // BindingDetails goes all the way back here for the save notification
    // and we have to go back down to the content to refresh the table
    if (currentSet >= 0) {
        BindingSetContent* current =  contents[currentSet];
        current->bindingSaved();
    }

    // save this for next time
    capturing = bindingDetails.isCapturing();
}

void BindingEditor::bindingCanceled()
{
    // regardless save this
    capturing = bindingDetails.isCapturing();
}

void BindingEditor::symbolTreeClicked(SymbolTreeItem* item)
{
    (void)item;
}

void BindingEditor::symbolTreeDoubleClicked(SymbolTreeItem* item)
{
    Symbol* s = item->getSymbol();
    if (s != nullptr) {
        Trace(1, "BindingEditor: Would very much like to add %s",
              s->getName());
    }
}
    
/**
 * Called by the Save button in the footer.
 *
 * BindingSetContent uses BindingSetDetails for editing and those
 * will have been committed by now, or if one was left open it is canceled.
 * Any changes that were made to existing bindings were left in the
 * same BindingSet that it was loaded from which we own.
 *
 * So the outer Save, just replaces the BindingSets we've been maintaining
 * in the SystemConfig.
 */
void BindingEditor::save()
{
    // make sure dialogs are clean
    setTable->cancel();
    for (auto cont : contents)
      cont->cancel();

    SystemConfig* scon = supervisor->getSystemConfig();
    BindingSets* master = scon->getBindings();

    master->transfer(bindingSets.get());
    supervisor->bindingEditorSave(master);
    
    bindingSets.reset();
    revertSets.reset();

    setTable->clear();
    contents.clear();
}

/**
 * Throw away all editing state.
 */
void BindingEditor::cancel()
{
    setTable->clear();
    
    for (auto cont : contents)
      cont->cancel();
    contents.clear();
    
    bindingSets.reset();
    revertSets.reset();
    
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

bool BindingEditor::checkName(BindingSet* set, juce::String newName, juce::StringArray& errors)
{
    bool ok = false;

    // todo: do some validation on the name
    bool validName = true;
    
    if (!validName) {
        errors.add(juce::String("Bindig Set name ") + newName + " contains illegal characters");
    }
    else {
        BindingSet* existing = bindingSets->find(newName);
        if (existing != nullptr && existing != set) {
            errors.add(juce::String("Binding Set name ") + newName + " is already in use");
        }
        else {
            ok = true;
        }
    }
    return ok;
}

void BindingEditor::bindingSetTableNew(BindingSet* neu, juce::StringArray& errors)
{
    if (checkName(nullptr, neu->name, errors)) {
        addNew(neu);
    }
    else {
        delete neu;
    }
}

void BindingEditor::addNew(BindingSet* neu)
{
    bindingSets->add(neu);

    install(neu);
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
    if (checkName(nullptr, newName, errors)) {
        BindingSet* set = getSourceBindingSet("Copy", errors);
        if (set != nullptr) {
            BindingSet* copy = new BindingSet(set);
            copy->name = newName;
            addNew(copy);
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
