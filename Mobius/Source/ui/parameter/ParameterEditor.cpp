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
    table->setBounds(area);
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
