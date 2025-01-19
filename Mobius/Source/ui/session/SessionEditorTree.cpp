/**
 * Extension of SymbolTree to browse session parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/SystemConfig.h"
#include "../../model/TreeForm.h"
#include "../../Provider.h"

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
        Trace(1, "SessionEditorTree: No tree definition %s", treename.toUTF8());
    }
    else {
        intern(&root, treedef);
    }
}

void SessionEditorTree::intern(SymbolTreeItem* parent, TreeNode* node)
{
    SymbolTreeItem* item = parent->internChild(node->name);

    // !! need more here
    item->setAnnotation(node->formName);
    
    for (auto child : node->nodes) {
        intern(item, child);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
