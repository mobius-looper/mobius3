/**
 * Extension of SymbolTree to browse session parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/TreeForm.h"

#include "SessionEditorTree.h"

SessionEditorTree::SessionEditorTree()
{
}

SessionEditorTree::~SessionEditorTree()
{
}

void SessionEditorTree::load(Provider* p, juce::String treename)
{
    SystemConfig* scon = p->getSystemConfig();
    TreeNode* treedef = scon->getTree(treename);
    if (treedef == nullptr) {
        Trace(1, "SessionEditorTree: No tree definition %s", treename);
    }
    else {
        // there are no expected root forms
        for (auto node : treedef->nodes) {

            SymbolTreeItem* item = root.internChild



    
    // todo: having trouble sticking things directly on the root
    
    
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
