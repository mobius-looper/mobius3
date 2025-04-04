
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/StaticConfig.h"
#include "../../model/TreeForm.h"
#include "../../model/Symbol.h"
#include "../../model/FunctionProperties.h"
#include "../../model/ParameterProperties.h"
#include "../../Provider.h"

#include "BindingTree.h"

BindingTree::BindingTree()
{
    //disableSearch();
}

BindingTree::~BindingTree()
{
}

/**
 * Set this if you want items in the tree to be draggable.
 */
void BindingTree::setDraggable(bool b)
{
    draggable = b;
}

void BindingTree::selectFirst()
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

void BindingTree::initialize(Provider* p)
{
    (void)root.internChild("Functions");
    (void)root.internChild("Scripts");
    (void)root.internChild("Parameters");
    (void)root.internChild("Configuration");
    (void)root.internChild("Samples");
    
    addFunctions(p);
    addParameters(p);

    SymbolTreeItem* params = root.internChild("Parameters");
    hideEmptyCategories(params);
}

void BindingTree::addParameters(Provider* p)
{
    StaticConfig* scon = p->getStaticConfig();
    TreeNode* treedef = scon->getTree("sessionCategory");
    SymbolTreeItem* parent = root.internChild("Parameters");

    if (treedef == nullptr)
      Trace(1, "BindingTree: Missing sessionCategory tree definition");
    else {
        for (auto node : treedef->nodes) {

            // category node
            SymbolTreeItem* category = parent->internChild(node->name);
            // this is used in static trees to identify the static form definition
            // for dynamic trees, we follow the same convention but since this is just
            // the name we don't need it
            category->setAnnotation(node->name);
            // all nodes can be clicked
            category->setNoSelect(false);

            juce::String formName = juce::String("sessionCategory") + node->name;
            TreeForm* form = scon->getTreeForm(formName);
            if (form == nullptr)
              Trace(1, "BindingTree: Missing form definition %s", formName.toUTF8());
            else {
                for (auto name : form->symbols) {
                    Symbol* s = p->getSymbols()->find(name);
                    if (s == nullptr)
                      Trace(1, "BindingTree: Invalid symbol name in tree definition %s",
                            name.toUTF8());
                    else {
                        ParameterProperties* props = s->parameterProperties.get();
                        if (props == nullptr)
                          Trace(1, "BindingTree: Symbol in tree definition not a parameter %s",
                                name.toUTF8());

                        // might be selectively filtered depending on use
                        else if (!isFiltered(s, props)) {

                            juce::String nodename = s->name;
                            if (props->displayName.length() > 0)
                              nodename = props->displayName;
            
                            SymbolTreeItem* param = new SymbolTreeItem(nodename);
                            param->setSymbol(s);

                            if (draggable) {
                                // for the description, use a prefix so the receiver
                                // knows where it came from followed by the canonical symbol name
                                param->setDragDescription(juce::String(DragPrefix) + s->name);
                            }
                            
                            category->addSubItem(param);
                        }
                    }
                }
            }
        }
    }
}

void BindingTree::addFunctions(Provider* p)
{
    SymbolTreeComparator comparator;
    SymbolTable* symbols = p->getSymbols();
    
    for (auto symbol : symbols->getSymbols()) {

        bool includeIt = !symbol->hidden;
        
        if (includeIt) {
        
            SymbolTreeItem* parent = nullptr;

            if (symbol->functionProperties != nullptr) {
                // this is what BindingTargetPanel does, why?
                if (symbol->behavior == BehaviorFunction)
                  parent = root.internChild("Functions");
                else
                  Trace(1, "SymbolTree: Symbol has function properties but not behavior %s",
                        symbol->getName());
            }
            else if (symbol->script != nullptr) {
                parent = root.internChild("Scripts");
            }
            else if (symbol->sample != nullptr) {
                parent = root.internChild("Samples");
            }
            else if (symbol->behavior == BehaviorActivation) {
                // only Overlays
                if (symbol->name.startsWith(Symbol::ActivationPrefixOverlay))
                    parent = root.internChild("Configuration");
            }
            
            if (parent != nullptr) {

                SymbolTreeItem* neu = new SymbolTreeItem(symbol->name);
                neu->setSymbol(symbol);
                if (draggable) {
                    // for the description, use a prefix so the receiver
                    // knows where it came from followed by the canonical symbol name
                    neu->setDragDescription(juce::String(DragPrefix) + symbol->name);
                }

                if (symbol->treePath == "") {
                    //parent->addSubItem(neu);
                    parent->addSubItemSorted(comparator, neu);
                }
                else {
                    juce::StringArray path = parsePath(symbol->treePath);
                    SymbolTreeItem* deepest = internPath(parent, path);
                    //deepest->addSubItem(neu);
                    deepest->addSubItemSorted(comparator, neu);
                }
            }
        }
    }
}

/**
 * Before adding a parameter Symbol to the tree, check for various filtering options.
 */
bool BindingTree::isFiltered(Symbol* s, ParameterProperties* props)
{
    (void)s;
    
    bool filtered = s->hidden || props->noBinding;

    return filtered;
}

/**
 * Since the parameter tree categories are populated from a TreeNode
 * rather than interning paths, filtering may cause some of them to be empty.
 * This applies only to the tree under the Parameters node.
 */
void BindingTree::hideEmptyCategories(SymbolTreeItem* node)
{
    int index = 0;
    while (index < node->getNumSubItems()) {
        SymbolTreeItem* item = static_cast<SymbolTreeItem*>(node->getSubItem(index));
        if (item->getNumSubItems() == 0) {
            node->removeSubItem(index, true);
        }
        else {
            index++;
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
