/**
 * Extension of SymbolTree to browse session parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "DynamicParameterTree.h"

DynamicParameterTree::DynamicParameterTree()
{
    //disableSearch();
    tree.setColour(juce::TreeView::ColourIds::backgroundColourId,
                   getLookAndFeel().findColour(juce::ListBox::backgroundColourId));
}

DynamicParameterTree::~DynamicParameterTree()
{
}

/**
 * Initialize the tree to contain all symbols from the global symbol table
 * that are marked for inclusion as default session parameters.
 *
 * Currently defined as any symbol that has a treePath, but may need more
 * restrictions on that.
 */
void DynamicParameterTree::initialize(Provider* p)
{
    SymbolTreeComparator comparator;

    internCategories();

    for (auto s : p->getSymbols()->getSymbols()) {

        if (s->parameterProperties != nullptr && s->treePath.length() > 0) {

            juce::StringArray path = parsePath(s->treePath);
            SymbolTreeItem* parent = internPath(&root, path);
            parent->setAnnotation(s->treePath);

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

/**
 * Intern the top-level parameter categories in an order that flows
 * better than alphabetical or as randomly encountered in a ValueSet.
 */
void DynamicParameterTree::internCategories()
{
    juce::StringArray categories ("Functions", "Quantize", "Switch", "Effects", "General", "Advanced");

    for (auto cat : categories) {
        SymbolTreeItem* item = root.internChild(cat);
        // this is used in static trees to identify the static form definition
        // for dynamic trees, we follow the same convention but since this is just
        // the name we don't need it
        item->setAnnotation(cat);
    }
}


/**
 * Initialize the tree to contain only those values in the provided
 * value set.
 */
void DynamicParameterTree::initialize(Provider* p, ValueSet* set)
{
    SymbolTreeComparator comparator;

    internCategories();
    
    juce::StringArray keys = set->getKeys();
    for (auto key : keys) {
        Symbol* s = p->getSymbols()->find(key);
        if (s == nullptr) {
            Trace(1, "ParamaeterEditorTree: Unknown symbol %s", key.toUTF8());
        }
        else if (s->parameterProperties == nullptr) {
            Trace(1, "DynamicParameterTree: Symbol is not a parameter %s", s->getName());
        }
        else {
            SymbolTreeItem* parent = nullptr;
            if (s->treePath == "") {
                // thought about lumping these into "Other" as a way to see symbols
                // that were missing the treePath, but loopCount is in there and this
                // moved to a primary session parameter and is already shown elsewhere
                // complain about them in the log instead
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

SymbolTreeItem* DynamicParameterTree::getFirst()
{
    SymbolTreeItem* first = static_cast<SymbolTreeItem*>(root.getSubItem(0));
    return first;
}

void DynamicParameterTree::selectFirst()
{
    SymbolTreeItem* first = getFirst();
    if (first != nullptr) {
        // asking for sendNotification means it will call
        // juce::TreeViewItem::itemSelectionChanged which SymbolTreeItem doesn't overload
        // and even if it did, we would need to avoid duplicating the response
        // to itemClicked which is what usually happens
        // just do it manually
        first->setSelected(true, false, juce::NotificationType::sendNotification);

        itemClicked(first);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Drag and Drop Shit
//
//////////////////////////////////////////////////////////////////////

/**
 * The only reason we're a DragAndDropTarget is so parameter forms can drag
 * fields off it to indiciate that the parameter should be removed from the form.
 * The usual weird control flow for Juce dnd.
 *
 * The only source we care about is ParmeterForm.
 */
bool DynamicParameterTree::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    bool interested = false;
    juce::Component* c = details.sourceComponent.get();

    // so...dynamic_cast is supposed to be evil, but we've got a problem here
    // how do you know what this thing is if all Juce gives you is a Component?
    // I suppose we could search upward and see if we are in the parent hierarchy.
    YanFieldLabel * l = dynamic_cast<YanFieldLabel*>(c);
    if (l == nullptr) {
        interested = true;
    }

    return interested;
}

void DynamicParameterTree::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    if (dropListener != nullptr)
      dropListener->dynamicParameterTreeDrop(this, details.description.toString());
}

void DynamicParameterTree::setDropListener(DropListener* l)
{
    dropListener = l;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
