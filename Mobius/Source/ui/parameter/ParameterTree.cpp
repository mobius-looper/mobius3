/**
 * Extension of SymbolTree to browse session parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/ValueSet.h"
#include "../../model/StaticConfig.h"
#include "../../model/TreeForm.h"
#include "../../Provider.h"

#include "ParameterTree.h"

ParameterTree::ParameterTree()
{
    //disableSearch();
}

ParameterTree::~ParameterTree()
{
}

/**
 * Set this if you want items in the tree to be draggable.
 * Usually true for the global static trees and false for
 * dynamic trees.
 */
void ParameterTree::setDraggable(bool b)
{
    draggable = b;
}

//////////////////////////////////////////////////////////////////////
//
// Common Interface
//
//////////////////////////////////////////////////////////////////////

SymbolTreeItem* ParameterTree::getFirst()
{
    SymbolTreeItem* first = static_cast<SymbolTreeItem*>(root.getSubItem(0));
    return first;
}

void ParameterTree::selectFirst()
{
    juce::TreeViewItem* first = root.getSubItem(0);
    if (first != nullptr) {
        // asking for sendNotification means it will call
        // juce::TreeViewItem::itemSelectionChanged which SymbolTreeItem doesn't overload
        // and even if it did, we would need to avoid duplicating the response
        // to itemClicked which is what usually happens
        // just do it manually by calling itemClicked
        first->setSelected(true, false, juce::NotificationType::sendNotification);
        
        SymbolTreeItem* sti = static_cast<SymbolTreeItem*>(first);
        itemClicked(sti);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Static Trees
//
//////////////////////////////////////////////////////////////////////

/**
 * Load a static tree given the name of a TreeNode from StaticConfig.
 *
 * This one requires a Provider because it needs access to the StaticConfig
 * for both the TreeNode definition, and the TreeForms it may reference since
 * the symbols in the tree nodes may come from the forms rather than the tree
 * definition.
 */
void ParameterTree::initializeStatic(Provider* p, juce::String treeName)
{
    StaticConfig* scon = p->getStaticConfig();
    TreeNode* treedef = scon->getTree(treeName);
    if (treedef == nullptr) {
        Trace(1, "SessionGlobalEditor: No tree definition %s", treeName.toUTF8());
    }
    else {
        // the root of the tree definition is not expected to be a useful form node
        // adding the children
        for (auto child : treedef->nodes) {
            intern(p, scon, &root, treeName, child);
        }
    }
}

void ParameterTree::intern(Provider* p, StaticConfig* scon, SymbolTreeItem* parent,
                           juce::String treePath, TreeNode* node)
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
        intern(p, scon, item, treePath, child);
    }

    // then symbols at this level
    // this is unusual and used only if you want to limit the included
    // symbols that would otherwise be defined in the form
    for (auto sname : node->symbols) {
        addSymbol(p, item, sname, "");
    }
    
    // usually the symbol list comes from the form
    if (node->symbols.size() == 0) {

        juce::String formName = item->getAnnotation();
        if (formName.length() > 0) {
            TreeForm* formdef = scon->getForm(formName);
            if (formdef != nullptr) {
                for (auto sname : formdef->symbols) {
                    // ignore special rendering symbols
                    if (!sname.startsWith("*"))
                      addSymbol(p, item, sname, formdef->suppressPrefix);
                }
            }
        }
    }
}

void ParameterTree::addSymbol(Provider* p, SymbolTreeItem* parent, juce::String name,
                              juce::String suppressPrefix)
{
    // SymbolTreeComparator comparator;
    Symbol* s = p->getSymbols()->find(name);
    if (s == nullptr)
      Trace(1, "ParameterTree: Invalid symbol name %s", name.toUTF8());
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
        chitem->setSymbol(s);
        // formerly sorted these, for static forms let the TreeForm control the order
        //parent->addSubItemSorted(comparator, chitem);
        parent->addSubItem(chitem);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Dynamic Trees
//
//////////////////////////////////////////////////////////////////////

/**
 * Initialize the tree to contain all symbols from the global symbol table
 * that are marked for inclusion as default session parameters.
 *
 * Currently defined as any symbol that has a treePath, but may need more
 * restrictions on that.
 */
void ParameterTree::initializeDynamic(Provider* p)
{
    SymbolTreeComparator comparator;

    // I think we can just assume this?
    draggable = true;

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
            neu->setSymbol(s);

            if (draggable) {
                // for the description, use a prefix so the receiver
                // knows where it came from followed by the canonical symbol name
                neu->setDragDescription(juce::String(DragPrefix) + s->name);
            }
            
            parent->addSubItemSorted(comparator, neu);
        }
    }
}

/**
 * Intern the top-level parameter categories in an order that flows
 * better than alphabetical or as randomly encountered in a ValueSet.
 */
void ParameterTree::internCategories()
{
    juce::StringArray categories ("Functions", "Sync", "Mixer", "Quantize", "Switch", "Effects", "General", "Advanced");

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
 *
 * NOT UESD
 *
 * This was an initial stab at making sparse trees with only those items
 * that corresponded to the values in a ValueSet.  Now that we always use
 * fully populated parameter trees for dynamic form building, this is no
 * longer used, but may come in handy someday.
 */
void ParameterTree::initializeSparse(Provider* p, ValueSet* set)
{
    SymbolTreeComparator comparator;

    internCategories();
    
    juce::StringArray keys = set->getKeys();
    for (auto key : keys) {
        Symbol* s = p->getSymbols()->find(key);
        if (s == nullptr) {
            Trace(1, "ParameterTree: Unknown symbol %s", key.toUTF8());
        }
        else if (s->parameterProperties == nullptr) {
            Trace(1, "ParameterTree: Symbol is not a parameter %s", s->getName());
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
