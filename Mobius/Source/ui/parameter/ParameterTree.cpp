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

/**
 * Set this if you want the tree builder to eliminate symbols flagged with the noDefault
 * option.
 */
void ParameterTree::setFilterNoDefault(bool b)
{
    filterNoDefault = b;
}

/**
 * Set this if you want the tree builder to eliminate symbols flagged with the noOverlay
 * option.
 */
void ParameterTree::setFilterNoOverlay(bool b)
{
    filterNoOverlay = b;
}

/**
 * Set this if the tree builder needs to exclude symbols that only apply to
 * specific track types.
 */
void ParameterTree::setTrackType(SymbolTrackType t)
{
    trackType = t;
}

//////////////////////////////////////////////////////////////////////
//
// Common Interface
//
//////////////////////////////////////////////////////////////////////

void ParameterTree::selectFirst()
{
    // If the tree is dynamic and contains hidden items with no children
    // the first one may not actually be visible
    SymbolTreeItem* first = nullptr;
    for (int i = 0 ; i < root.getNumSubItems() ; i++) {
        SymbolTreeItem* item = static_cast<SymbolTreeItem*>(root.getSubItem(i));
        if (!item->isHidden()) {
            first = item;
            break;
        }
    }
    
    if (first != nullptr) {
        // asking for sendNotification means it will call
        // juce::TreeViewItem::itemSelectionChanged which SymbolTreeItem doesn't overload
        // and even if it did, we would need to avoid duplicating the response
        // to itemClicked which is what usually happens
        // just do it manually by calling itemClicked
        first->setSelected(true, false, juce::NotificationType::sendNotification);
        itemClicked(first);
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
            TreeForm* formdef = scon->getTreeForm(formName);
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
    StaticConfig* scon = p->getStaticConfig();
    
    internCategories();

    for (auto s : p->getSymbols()->getSymbols()) {

        ParameterProperties* props = s->parameterProperties.get();

        if (props != nullptr && s->treePath.length() > 0) {

            if (!isFiltered(s, props)) {

                juce::StringArray path = parsePath(s->treePath);
                SymbolTreeItem* parent = internPath(&root, path);
            
                parent->setAnnotation(s->treePath);
                parent->setNoSelect(false);

                // so much string slinging, could cache these
                juce::String formName = juce::String("sessionCategory") + s->treePath;
                TreeForm* form = scon->getTreeForm(formName);
                ParameterTreeComparator comparator (form);
                
                juce::String nodename = s->name;
                if (props->displayName.length() > 0)
                  nodename = props->displayName;
            
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

    hideEmptyCategories();
}

/**
 * Before adding a parameter Symbol to the tree, check for various filtering options.
 */
bool ParameterTree::isFiltered(Symbol* s, ParameterProperties* props)
{
    bool filtered = false;

    // first the noDefault option
    if (filterNoDefault)
      filtered = props->noDefault;

    if (!filtered && filterNoOverlay)
      filtered = props->noOverlay;

    // then track types
    if (!filtered && trackType != TrackTypeNone) {
        if (s->trackTypes.size() > 0) {
            filtered = !s->trackTypes.contains(trackType);
        }
    }
    return filtered;
}

/**
 * Intern the top-level parameter categories in an order that flows
 * better than alphabetical or as randomly encountered in a ValueSet.
 */
void ParameterTree::internCategories()
{
    juce::StringArray categories ("General", "Ports", "Midi", "Sync", "Mixer", "Follow", "Quantize", "Switch", "Functions", "Effects", "Advanced","Overlay");

    for (auto cat : categories) {
        SymbolTreeItem* item = root.internChild(cat);
        // this is used in static trees to identify the static form definition
        // for dynamic trees, we follow the same convention but since this is just
        // the name we don't need it
        item->setAnnotation(cat);
        // all nodes can be clicked
        item->setNoSelect(false);
    }
}

/**
 * After popuplating a dynamic form, remove any catagories that ended up with
 * nothing in them due to exclusion options in the symbols.
 * Technically, this should traverse looking for categories more than one level deep
 * but right now the only ones of concern are at the top.
 */
void ParameterTree::hideEmptyCategories()
{
    for (int i = 0 ; i < root.getNumSubItems() ; i++) {
        SymbolTreeItem* item = static_cast<SymbolTreeItem*>(root.getSubItem(i));
        if (item->getNumSubItems() == 0) {
            item->setHidden(true);
        }
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
    
    juce::StringArray keys;
    set->getKeys(keys);
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

    hideEmptyCategories();
}

/**
 * Special node sorter that is guided by a TreeForm definition.
 */
ParameterTreeComparator::ParameterTreeComparator(TreeForm* tf)
{
    form = tf;
}

int ParameterTreeComparator::compareElements(juce::TreeViewItem* first, juce::TreeViewItem* second)
{
    int result = 0;
    if (form == nullptr || form->symbols.size() == 0) {
        // same as alphabetic comparator in SymbolTree
        juce::String name1 = (static_cast<SymbolTreeItem*>(first))->getName();
        juce::String name2 = (static_cast<SymbolTreeItem*>(second))->getName();
        result = name1.compareIgnoreCase(name2);
    }
    else {
        Symbol* s1 = (static_cast<SymbolTreeItem*>(first))->getSymbol();
        Symbol* s2 = (static_cast<SymbolTreeItem*>(second))->getSymbol();
        // these should NOT be null, but don't die
        if (s1 != nullptr && s2 != nullptr) {
            int index1 = form->symbols.indexOf(s1->name);
            int index2 = form->symbols.indexOf(s2->name);
            if (index1 < 0) {
                // not on the list put at the end
                result = 1;
            }
            else if (index1 < index2)
              result = -1;
            else if (index1 > index2)
              result = 1;
        }
    }
    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
