/**
 * ConfigEditor for the ParameterSets.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/ParameterSets.h"
#include "../../model/ValueSet.h"
#include "../../Supervisor.h"
#include "../../Provider.h"

#include "ParameterSetTable.h"

#include "ParameterEditor.h"

ParameterEditor::ParameterEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("ParameterEditor");

    table.reset(new ParameterSetTable(s));
    addAndMakeVisible(table.get());

    table->setListener(this);
}

ParameterEditor::~ParameterEditor()
{
}

void ParameterEditor::prepare()
{
}

void ParameterEditor::resized()
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

void ParameterEditor::load()
{
    ParameterSets* master = supervisor->getParameterSets();
    parameters.reset(new ParameterSets(master));
    revertParameters.reset(new ParameterSets(master));

    table->load(parameters.get());

    for (auto set : parameters->sets) {
        DynamicTreeForms* dtf = new DynamicTreeForms();
        dtf->initialize(supervisor, set);
        treeForms.add(dtf);
        addChildComponent(dtf);
    }

    table->selectRow(0);
    show(0);
    resized();
}

void ParameterEditor::show(int index)
{
    if (index != currentSet) {
        if (currentSet >= 0) {
            DynamicTreeForms* existing = treeForms[currentSet];
            existing->setVisible(false);
        }
        DynamicTreeForms* neu = treeForms[index];
        if (neu != nullptr) {
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
void ParameterEditor::save()
{
    // save any forms that were built and displayed back to the ValueSets
    // in our copied ParameterSets
    for (auto tf : treeForms)
      tf->save();

    // rebuild the list for the master ParameterSets container
    ParameterSets* master = supervisor->getParameterSets();
    master->sets.clear();

    // transfer ownership of the sets we had under our control
    while (parameters->sets.size() > 0) {
        ValueSet* vs = parameters->sets.removeAndReturn(0);
        master->sets.add(vs);
    }

    supervisor->updateParameterSets();
}

/**
 * Throw away all editing state.
 */
void ParameterEditor::cancel()
{
}

void ParameterEditor::decacheForms()
{
}

void ParameterEditor::revert()
{
    parameters.reset(new ParameterSets(revertParameters.get()));
}

//////////////////////////////////////////////////////////////////////
//
// ParameterSet Table
//
//////////////////////////////////////////////////////////////////////

/**
 * This is called when the selected row changes either by clicking on
 * it or using the keyboard arrow keys after a row has been selected.
 */
void ParameterEditor::typicalTableChanged(TypicalTable* t, int row)
{
    (void)t;
    if (row < 0) {
        Trace(1, "SessionTrackEditor: Change alert with no selected track number");
    }
    else {
        show(row);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
