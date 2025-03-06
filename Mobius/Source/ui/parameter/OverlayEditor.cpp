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

    table.reset(new OverlayTable(s));
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

    table->load(overlays.get());

    for (auto set : overlays->getSets()) {
        OverlayTreeForms* otf = new OverlayTreeForms();
        otf->initialize(supervisor);
        otf->load(set);
        treeForms.add(otf);
        addChildComponent(otf);
    }

    table->selectRow(0);
    show(0);
    resized();
}

void OverlayEditor::show(int index)
{
    if (index != currentSet) {
        if (currentSet >= 0) {
            OverlayTreeForms* existing = treeForms[currentSet];
            existing->setVisible(false);
        }
        OverlayTreeForms* neu = treeForms[index];
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
}

/**
 * Throw away all editing state.
 */
void OverlayEditor::cancel()
{
}

void OverlayEditor::decacheForms()
{
}

void OverlayEditor::revert()
{
    overlays.reset(new ParameterSets(revertOverlays.get()));
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
