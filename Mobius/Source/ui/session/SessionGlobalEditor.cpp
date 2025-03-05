/**
 * SessionEditor subcomponent for editing the global sesison parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/StaticConfig.h"
#include "../../model/TreeForm.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "../parameter/SymbolTree.h"
#include "../parameter/ParameterForm.h"
#include "../parameter/ParameterTree.h"
#include "../parameter/ParameterFormCollection.h"

#include "SessionGlobalEditor.h"

SessionGlobalEditor::SessionGlobalEditor()
{
}

void SessionGlobalEditor::initialize(Provider* p)
{
    provider = p;

    // this is used by the inherited symbolTreeClicked method
    // to generate form names when the tree is clicked if the clicked
    // node didn't specify one
    treeName = juce::String(TreeName);
    
    tree.initializeStatic(provider, treeName);

    // this wants a ValueSet but we don't get that until load
    forms.initialize(p, this, nullptr);
}

void SessionGlobalEditor::load(ValueSet* src)
{
    forms.load(src);
}

void SessionGlobalEditor::save(ValueSet* dest)
{
    forms.save(dest);
}

void SessionGlobalEditor::cancel()
{
    forms.cancel();
}

void SessionGlobalEditor::decacheForms()
{
    forms.decache();
}

/**
 * Global editor is guided by the static form definitions.
 */
ParameterForm* SessionGlobalEditor::parameterFormCollectionCreate(juce::String formName)
{
    ParameterForm* form = nullptr;
    
    StaticConfig* scon = provider->getStaticConfig();
    TreeForm* formdef = scon->getForm(formName);
    if (formdef == nullptr) {
        Trace(1, "SessionGlobalEditor: Unknown form %s", formName.toUTF8());
    }
    else {
        form = new ParameterForm();

        juce::String title = formdef->title;
        if (title.length() == 0) title = formName;

        form->setTitle(title);

        form->build(provider, formdef);
    }
    return form;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
