/**
 * Extension of SymbolTree to browse session parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "ParameterEditorTree.h"

ParameterEditorTree::ParameterEditorTree()
{
    //disableSearch();
    tree.setColour(juce::TreeView::ColourIds::backgroundColourId,
                   getLookAndFeel().findColour(juce::ListBox::backgroundColourId));
}

ParameterEditorTree::~ParameterEditorTree()
{
}

void ParameterEditorTree::load(Provider* p, ValueSet* set)
{
    provider = p;
    SymbolTreeComparator comparator;

    juce::StringArray keys = set->getKeys();
    for (auto key : keys) {
        Symbol* s = p->getSymbols()->find(key);
        if (s == nullptr) {
            Trace(1, "ParamaeterEditorTree: Unknown symbol %s", key.toUTF8());
        }
        else {
            SymbolTreeItem* parent = nullptr;
            if (s->treePath == "") {
                parent = root.internChild("Other");
                parent->setAnnotation("Other");
            }
            else {
                juce::StringArray path = parsePath(s->treePath);
                parent = internPath(&root, path);
                parent->setAnnotation(s->treePath);
            }

            parent->setNoSelect(false);

            juce::String nodename = s->name;
            if (s->parameterProperties != nullptr)
              nodename = s->parameterProperties->displayName;
            SymbolTreeItem* neu = new SymbolTreeItem(nodename);
            // put the symbol on the child so we can get to them aalready sorted
            neu->addSymbol(s);
            parent->addSubItemSorted(comparator, neu);
            
        }
    }
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
