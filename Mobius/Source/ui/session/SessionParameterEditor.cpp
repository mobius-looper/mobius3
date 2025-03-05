/**
 * SessionEditor subcomponent for editing the global sesison parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "../parameter/SymbolTree.h"
#include "../parameter/ParameterForm.h"
#include "../parameter/ParameterTree.h"
#include "../parameter/ParameterFormCollection.h"

#include "SessionParameterEditor.h"

SessionParameterEditor::SessionParameterEditor()
{
}

void SessionParameterEditor::initialize(Provider* p)
{
    provider = p;
    
    tree.initializeDynamic(provider);
    // get clicks from the tree
    // the default parameter tree forms are fully popuplated and
    // don't need to support drag and drop
    tree.setListener(this);
    
    // this wants a ValueSet but we don't get that until load
    // rethink this interface, if we never have the ValueSet on
    // initialize then don't pass it
    forms.initialize(p, this, nullptr);
}

void SessionParameterEditor::load(ValueSet* src)
{
    forms.load(src);
}

void SessionParameterEditor::save(ValueSet* dest)
{
    forms.save(dest);
}

void SessionParameterEditor::cancel()
{
    forms.cancel();
}

void SessionParameterEditor::decacheForms()
{
    forms.decache();
}

/**
 * The session parameter editor forms are not dynamic, they will contain
 * all of the parameters defined within a category.  The category name is the
 * passed formName.
 *
 * There are two ways we could get the symbols to add.  1) find the category in the
 * ParameterTree and look at the sub-items or 2) iterate over the SymbolTable looking
 * for symbols in this category.
 *
 * Since the tree might do filtering, let the tree decide.
 */
ParameterForm* SessionParameterEditor::parameterFormCollectionCreate(juce::String formName)
{
    ParameterForm* form = nullptr;

    // by convention we put the formName or "category" name on the item annotation
    // the same annotation will be set on the sub-items so this searcher
    // needs to stp at the highest level node that has this annotation
    SymbolTreeItem* parent = tree.findAnnotatedItem(formName);
    if (parent == nullptr) {
        Trace(1, "SessionParameterEditor: No tree node with annotation %s", formName.toUTF8());
    }
    else {
        form = new ParameterForm();
        
        // todo: form title

        // so we can iterate over the children, but the parent node should also
        // have an Array of the child symbols as well, right?
        for (int i = 0 ; i < parent->getNumSubItems() ; i++) {
            SymbolTreeItem* item = static_cast<SymbolTreeItem*>(parent->getSubItem(i));
            Symbol* s = item->getSymbol();
            if (s == nullptr)
              Trace(1, "SessionParameterEditor: Tree node without symbol %s",
                    item->getName().toUTF8());
            else
              form->add(provider, s, values);
        }
    }
    return form;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
