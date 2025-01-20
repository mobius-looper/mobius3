/**
 * SessionEditor subcomponent for editing the global sesison parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "SessionTreeForms.h"

#include "SessionGlobalEditor.h"

SessionGlobalEditor::SessionGlobalEditor()
{
    addAndMakeVisible(&forms);
}

void SessionGlobalEditor::initialize(Provider* p)
{
    forms.initialize(p, juce::String("sessionGlobal"));
}

void SessionGlobalEditor::cancel()
{
    forms.cancel();
}

void SessionGlobalEditor::decacheForms()
{
    forms.decache();
}

void SessionGlobalEditor::resized()
{
    forms.setBounds(getLocalBounds());
}

void SessionGlobalEditor::load(ValueSet* src)
{
    forms.load(src);
}

void SessionGlobalEditor::save(ValueSet* dest)
{
    forms.save(dest);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
