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
    provider = p;
    
    StaticConfig* scon = p->getStaticConfig();
    TreeNode* treedef = scon->getTree(treename);
    if (treedef == nullptr) {
        Trace(1, "SessionEditorTree: No tree definition %s", treename.toUTF8());
    }
    else {
        // the root of the tree definition is not expected to be a useful form node
        // adding the children
        for (auto child : treedef->nodes) {
            intern(&root, treename, child);
        }
    }
}

SymbolTreeItem* SessionEditorTree::getFirst()
{
    SymbolTreeItem* first = static_cast<SymbolTreeItem*>(root.getSubItem(0));
    return first;
}

void SessionEditorTree::selectFirst()
{
    juce::TreeViewItem* first = root.getSubItem(0);
    if (first != nullptr) {
        // hmm, if you allowed this to send a notification would that cause the
        // symbolTreeClickk callback to fire?  If so might simplifiy the way
        // SessionTreeForm opens the initial from after startup
        first->setSelected(true, false, juce::NotificationType::sendNotification);
    }
}

void SessionEditorTree::intern(SymbolTreeItem* parent, juce::String treePath, TreeNode* node)
{
    SymbolTreeItem* item = parent->internChild(node->name);
    treePath = treePath + node->name;

    juce::String nodeForm = node->formName;
    if (nodeForm.length() == 0)
      item->setAnnotation(treePath);
    else if (nodeForm != "none")
      item->setAnnotation(node->formName);

    // all nodes can be clicked
    item->setNoSelect(false);

    // first the sub-categories
    for (auto child : node->nodes) {
        intern(item, treePath, child);
    }

    // then symbols at this level
    // this is unusual and used only if you want to limit the included
    // symbols that would otherwise be defined in the form
    for (auto sname : node->symbols) {
        addSymbol(item, sname);
    }
    
    // usually the symbol list comes from the form
    if (node->symbols.size() == 0) {

        juce::String formName = item->getAnnotation();
        if (formName.length() > 0) {
            StaticConfig* scon = provider->getStaticConfig();
            TreeForm* formdef = scon->getForm(formName);
            if (formdef != nullptr) {
                for (auto sname : formdef->symbols) {
                    addSymbol(item, sname);
                }
            }
        }
    }
}

void SessionEditorTree::addSymbol(SymbolTreeItem* parent, juce::String name)
{
    SymbolTreeComparator comparator;
    
    Symbol* s = provider->getSymbols()->find(name);
    if (s == nullptr)
      Trace(1, "SessionEditorTree: Invalid symbol name %s", name.toUTF8());
    else {
        parent->addSymbol(s);
        SymbolTreeItem* chitem = new SymbolTreeItem(name);
        parent->addSubItemSorted(comparator, chitem);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
