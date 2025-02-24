/**
 * Extension of SymbolTree to browse session parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/TreeForm.h"
#include "../../Provider.h"

#include "ParameterEditorTree.h"

ParameterEditorTree::ParameterEditorTree()
{
    //disableSearch();
}

ParameterEditorTree::~ParameterEditorTree()
{
}

void ParameterEditorTree::load(Provider* p, ValueSet* set)
{
    provider = p;

    (void)set;

    root.internChild("Test");
}

SymbolTreeItem* ParameterEditorTree::getFirst()
{
    SymbolTreeItem* first = static_cast<SymbolTreeItem*>(root.getSubItem(0));
    return first;
}

void ParameterEditorTree::selectFirst()
{
    juce::TreeViewItem* first = root.getSubItem(0);
    if (first != nullptr) {
        // hmm, if you allowed this to send a notification would that cause the
        // symbolTreeClickk callback to fire?  If so might simplifiy the way
        // SessionTreeForm opens the initial from after startup
        first->setSelected(true, false, juce::NotificationType::sendNotification);
    }
}

#if 0
void ParameterEditorTree::intern(SymbolTreeItem* parent, juce::String treePath, TreeNode* node)
{
    SymbolTreeItem* item = parent->internChild(node->name);
    treePath = treePath + node->name;

    juce::String nodeForm = node->formName;
    if (nodeForm.length() == 0)
      item->setAnnotation(treePath);
    else 
      item->setAnnotation(nodeForm);

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
        addSymbol(item, sname, "");
    }
    
    // usually the symbol list comes from the form
    if (node->symbols.size() == 0) {

        juce::String formName = item->getAnnotation();
        if (formName.length() > 0) {
            StaticConfig* scon = provider->getStaticConfig();
            TreeForm* formdef = scon->getForm(formName);
            if (formdef != nullptr) {
                for (auto sname : formdef->symbols) {
                    // ignore special rendering symbols
                    if (!sname.startsWith("*"))
                      addSymbol(item, sname, formdef->suppressPrefix);
                }
            }
        }
    }
}

void ParameterEditorTree::addSymbol(SymbolTreeItem* parent, juce::String name,
                                  juce::String suppressPrefix)
{
//    SymbolTreeComparator comparator;
    
    Symbol* s = provider->getSymbols()->find(name);
    if (s == nullptr)
      Trace(1, "ParameterEditorTree: Invalid symbol name %s", name.toUTF8());
    else {
        // don't think this part is necessary
        parent->addSymbol(s);

        // it doesn't really matter what the name of this is, the important
        // part is the annotation of the parent node, which is the form reference
        juce::String nodename = name;
        if (s->parameterProperties != nullptr) {
            nodename = s->parameterProperties->displayName;
            if (suppressPrefix.length() > 0 && nodename.indexOf(suppressPrefix) == 0) {
                nodename = nodename.fromFirstOccurrenceOf(suppressPrefix + " ", false, false);
            }
        }
        
        SymbolTreeItem* chitem = new SymbolTreeItem(nodename);
        //parent->addSubItemSorted(comparator, chitem);
        parent->addSubItem(chitem);
    }
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
