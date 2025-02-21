/**
 * SessionEditor subcomponent for editing the global sesison parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "SessionTreeForms.h"

#include "SessionFunctionEditor.h"

SessionFunctionEditor::SessionFunctionEditor()
{
    addAndMakeVisible(&forms);
}

void SessionFunctionEditor::initialize(Provider* p)
{
    forms.initialize(p, juce::String("sessionFunction"));
}

void SessionFunctionEditor::cancel()
{
    forms.cancel();
}

void SessionFunctionEditor::decacheForms()
{
    forms.decache();
}

void SessionFunctionEditor::resized()
{
    forms.setBounds(getLocalBounds());
}

void SessionFunctionEditor::load(ValueSet* src)
{
    forms.load(src);
}

void SessionFunctionEditor::save(ValueSet* dest)
{
    forms.save(dest);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
