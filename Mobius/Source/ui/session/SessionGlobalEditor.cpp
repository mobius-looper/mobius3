/**
 * SessionEditor subcomponent for editing the global sesison parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/StaticConfig.h"
#include "../../model/TreeForm.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "SymbolTree.h"
#include "ParameterForm.h"
#include "ParameterTree.h"
#include "ParameterFormCollection.h"

#include "SessionGlobalEditor.h"

SessionGlobalEditor::SessionGlobalEditor()
{
}

void SessionGlobalEditor::initialize(Provider* p)
{
    provider = p;
    
    tree.initializeStatic(provider, juce::String(TreeName));
    
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
 * When a tree node is clicked, ask the form collection to display the form.
 */
void SessionGlobalEditor::symbolTreeClicked(SymbolTreeItem* item)
{
    SymbolTreeItem* container = item;
    
    // if this is a leaf node, go up to the parent and show the entire parent form
    if (item->getNumSubItems() == 0) {
        container = static_cast<SymbolTreeItem*>(item->getParentItem());
    }

    // SymbolTreeItem is a generic model that doesn't understand it's usage
    // for form delivery, the tree builder left the form name as the "annotation"
    
    juce::String formName = container->getAnnotation();
    if (formName.length() == 0) {
        // default form name is a combination of the tree name and node name
        formName = juce::String(TreeName) + item->getName();
    }
    
    forms.show(formName);
}

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
