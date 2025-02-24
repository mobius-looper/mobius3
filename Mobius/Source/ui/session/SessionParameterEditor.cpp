/**
 * SessionEditor subcomponent for editing the global sesison parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "SessionTreeForms.h"

#include "SessionParameterEditor.h"

SessionParameterEditor::SessionParameterEditor()
{
    addAndMakeVisible(&forms);
}

void SessionParameterEditor::initialize(Provider* p)
{
    forms.initialize(p);
}

void SessionParameterEditor::cancel()
{
    forms.cancel();
}

void SessionParameterEditor::decacheForms()
{
    forms.decache();
}

void SessionParameterEditor::resized()
{
    forms.setBounds(getLocalBounds());
}

void SessionParameterEditor::load(ValueSet* src)
{
    forms.load(src);
}

void SessionParameterEditor::save(ValueSet* dest)
{
    forms.save(dest);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
