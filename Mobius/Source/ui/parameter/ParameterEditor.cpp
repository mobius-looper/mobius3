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
        ParameterTreeForms* ptf = new ParameterTreeForms();
        ptf->initialize(supervisor, set);
        treeForms.add(ptf);
        addChildComponent(ptf);
    }

    show(0);
    resized();
}

void ParameterEditor::show(int index)
{
    if (index != currentSet) {
        if (currentSet >= 0) {
            ParameterTreeForms* existing = treeForms[currentSet];
            existing->setVisible(false);
        }
        ParameterTreeForms* neu = treeForms[index];
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
 */
void ParameterEditor::save()
{
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
    (void)row;
    
    int newRow = table->getSelectedRow();
    if (newRow < 0) {
        Trace(1, "SessionTrackEditor: Change alert with no selected track number");
    }

#if 0    
    else if (newRow != currentSet) {
        
        saveForms(currentTrack);
        
        loadForms(newRow);
        
        currentTrack = newRow;
    }
#endif
    
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
