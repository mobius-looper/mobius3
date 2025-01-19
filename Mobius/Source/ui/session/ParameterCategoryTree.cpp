// OBSOLETE: DELETE


/**
 *
 * Extension of SymbolTree to browse categories for the SessionEditor
 * 
 * See here for simple example
 *
 * https://forum.juce.com/t/treeview-tutorial/20187/6
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"

#include "ParameterCategoryTree.h"

//////////////////////////////////////////////////////////////////////
//
// Tree
//
//////////////////////////////////////////////////////////////////////

ParameterCategoryTree::ParameterCategoryTree()
{
}

ParameterCategoryTree::~ParameterCategoryTree()
{
}

void ParameterCategoryTree::load(SymbolTable* symbols, juce::String includeCsv)
{
    SymbolTreeComparator comparator;

    juce::StringArray includes = juce::StringArray::fromTokens(includeCsv, ",");

    SymbolTreeItem* parametersNode = root.internChild("Parameters");
    // start with the root open
    parametersNode->setOpen(true);
    
    for (auto symbol : symbols->getSymbols()) {

        // this can only include symbols that have a treePath, though I suppose for the
        // ones that don't we could add an "Other" category

        bool included = (includes.size() == 0 || includes.contains(symbol->treeInclude));
        
        if (included && !symbol->hidden) {

            SymbolTreeItem* neu = new SymbolTreeItem(symbol->name);
            // the leaf node doesn't need the symbol, but that could
            // be another, less convenient way to store it
            neu->addSymbol(symbol);

            // unlike the base SymbolTree, we have no special parenting
            SymbolTreeItem* parent = parametersNode;

            if (symbol->treePath == "") {
                //parent->addSubItem(neu);
                parent->addSubItemSorted(comparator, neu);
            }
            else {
                juce::StringArray path = parsePath(symbol->treePath);
                SymbolTreeItem* deepest = internPath(parent, path);
                //deepest->addSubItem(neu);
                deepest->addSubItemSorted(comparator, neu);

                // interior nodes default to unselectable
                deepest->setNoSelect(false);

                // once these are selectable, they default to white like
                // the leaf nodes
                deepest->setColor(juce::Colours::blue);

                // this is where it's more convenient to have the symbol
                // list though we could iterate over the children too
                deepest->addSymbol(symbol);
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
