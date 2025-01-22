/**
 * Extension of SymbolTree to browse session parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/StaticConfig.h"
#include "../../model/TreeForm.h"
#include "../../Provider.h"

#include "SessionEditorTree.h"

SessionEditorTree::SessionEditorTree()
{
    disableSearch();
}

SessionEditorTree::~SessionEditorTree()
{
}

void SessionEditorTree::load(Provider* p, juce::String treename)
{
    StaticConfig* scon = p->getStaticConfig();
    TreeNode* treedef = scon->getTree(treename);
    if (treedef == nullptr) {
        Trace(1, "SessionEditorTree: No tree definition %s", treename.toUTF8());
    }
    else {
        // the root of the tree definition is not expected to be a useful form node
        // adding the children
        for (auto child : treedef->nodes) {
            intern(&root, child);
        }
    }
}

void SessionEditorTree::intern(SymbolTreeItem* parent, TreeNode* node)
{
    SymbolTreeItem* item = parent->internChild(node->name);

    // !! need more here
    item->setAnnotation(node->formName);

    // all nodes can be clicked
    item->setNoSelect(false);
    
    for (auto child : node->nodes) {
        intern(item, child);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
