/**
 * See here for simple example
 *   https://forum.juce.com/t/treeview-tutorial/20187/6
 *
 * This is the basis for tree view where node associated with Symbols
 * or categories of symbols.
 *
 * Not used directly, extended by ParameterTree.
 *
 * The first implementation of this walked over the SymbolTable and built
 * a tree for everything, both functions and parameters.  This is no longer used.
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../script/MslValue.h"

#include "SymbolTree.h"

//////////////////////////////////////////////////////////////////////
//
// Construction
//
//////////////////////////////////////////////////////////////////////

SymbolTree::SymbolTree()
{
    setLookAndFeel(&laf);
    
    addAndMakeVisible(tree);
    tree.setRootItem(&root);
    tree.setRootItemVisible(false);
    
    addAndMakeVisible(search);
    search.setListener(this);
}

SymbolTree::~SymbolTree()
{
    setLookAndFeel(nullptr);
}

void SymbolTree::setListener(Listener* l)
{
    listener = l;
}

void SymbolTree::setDropListener(DropTreeView::Listener* l)
{
    tree.setListener(l);
}

SymbolTree::LookAndFeel::LookAndFeel(SymbolTree* st)
{
    symbolTree = st;
}

void SymbolTree::LookAndFeel::drawTreeviewPlusMinusBox (juce::Graphics& g,
                                                        const juce::Rectangle<float>& area,
                                                        juce::Colour backgroundColour,
                                                        bool isOpen, bool isMouseOver)
{
    (void)backgroundColour;
    (void)isMouseOver;
    juce::Path p;
    p.addTriangle (0.0f, 0.0f, 1.0f, isOpen ? 0.0f : 0.5f, isOpen ? 0.5f : 0.0f, 1.0f);

    // g.setColour (backgroundColour.contrasting().withAlpha (isMouseOver ? 0.5f : 0.3f));
    g.setColour (juce::Colours::white);
    g.fillPath (p, p.getTransformToScaleToFit (area.reduced (2, area.getHeight() / 4), true));
}

void SymbolTree::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    if (!searchDisabled)
      search.setBounds(area.removeFromTop(22));
    tree.setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// Building
//
//////////////////////////////////////////////////////////////////////

SymbolTreeItem* SymbolTree::internPath(SymbolTreeItem* parent, juce::StringArray path)
{
    SymbolTreeItem* level = parent;
    for (auto node : path) {
        level = level->internChild(node);
    }
    return level;
}

juce::StringArray SymbolTree::parsePath(juce::String s)
{
    return juce::StringArray::fromTokens(s, "/", "");
}

void SymbolTree::unhide(SymbolTreeItem* node)
{
    node->setHidden(false);
    if (node->isSelected())
      node->setSelected(false, false, juce::NotificationType::sendNotification);
    for (int i = 0 ; i < node->getNumSubItems() ; i++) {
        SymbolTreeItem* item = static_cast<SymbolTreeItem*>(node->getSubItem(i));
        unhide(item);
    }
}

/**
 * This is called by one of the items.
 * Can either have a listener here or the subclass can override it.
 */
void SymbolTree::itemClicked(SymbolTreeItem* item)
{
    if (item->canBeSelected()) {
        //Trace(2, "Clicked %s", item->getName().toUTF8());
        if (listener != nullptr)
          listener->symbolTreeClicked(item);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Old Load Interface
// Remove this
//
//////////////////////////////////////////////////////////////////////

void SymbolTree::loadSymbols(SymbolTable* symbols, juce::String newFavorites)
{
    loadSymbols(symbols, newFavorites, "");
}
    
void SymbolTree::loadSymbols(SymbolTable* symbols, juce::String newFavorites,
                             juce::String includeCsv)
{
    SymbolTreeComparator comparator;

    juce::StringArray includes = juce::StringArray::fromTokens(includeCsv, ",", "");
    
    // pre-intern these in a particular order
    SymbolTreeItem* favoritesNode = root.internChild("Favorites");

    // kludge: if an includes list is passed assume we want only parameters
    // ideally we should be able to intern the root nodes to lock their order,
    // but have them be invisible, then visible them if things are added underneath
    if (includeCsv.length() > 0) {
        (void)root.internChild("Parameters");
    }
    else {
        (void)root.internChild("Functions");
        (void)root.internChild("Parameters");
        (void)root.internChild("Controls");
        (void)root.internChild("Scripts");
        (void)root.internChild("Structures");
        (void)root.internChild("Samples");
        (void)root.internChild("Other");
    }
    
    favorites.clear();
    if (newFavorites.length() > 0) {
        // todo: may need to filter these if we remove symbols
        favorites = juce::StringArray::fromTokens(newFavorites, ",", "");
        for (auto name : favorites) {
            SymbolTreeItem* neu = new SymbolTreeItem(name);
            favoritesNode->addSubItemSorted(comparator, neu);
        }
    }
    
    for (auto symbol : symbols->getSymbols()) {

        bool includeIt = !symbol->hidden &&
            (includes.size() == 0 || includes.contains(symbol->treeInclude));
        
        
        if (includeIt) {
        
            SymbolTreeItem* parent = nullptr;

            if (symbol->parameterProperties != nullptr) {
                if (symbol->parameterProperties->control)
                  parent = root.internChild("Controls");
                else
                  parent = root.internChild("Parameters");
            }
            else if (symbol->functionProperties != nullptr) {
                // this is what BindingTargetPanel does, why?
                if (symbol->behavior == BehaviorFunction)
                  parent = root.internChild("Functions");
                else
                  Trace(1, "SymbolTree: Symbol has function properties but not behavior %s",
                        symbol->getName());
            }
            else if (symbol->script != nullptr)
              parent = root.internChild("Scripts");
            else if (symbol->sample != nullptr)
              parent = root.internChild("Samples");
            else if (symbol->behavior == BehaviorActivation)
              parent = root.internChild("Structures");
            else
              parent = root.internChild("Other");

            if (parent != nullptr) {

                SymbolTreeItem* neu = new SymbolTreeItem(symbol->name);

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

//////////////////////////////////////////////////////////////////////
//
// Search
//
//////////////////////////////////////////////////////////////////////

void SymbolTree::disableSearch()
{
    removeChildComponent(&search);
    searchDisabled = true;
}

void SymbolTree::inputEditorShown(YanInput*)
{
    startSearch();
}

void SymbolTree::inputEditorChanged(YanInput* input, juce::String text)
{
    (void)input;
    //Trace(2, "%s", text.toUTF8());
    searchTree(text, &root);
}

void SymbolTree::inputEditorHidden(YanInput*)
{
    endSearch();
}

void SymbolTree::startSearch()
{
    // since we don't unhide then the search is ended, do it now
    unhide(&root);
    
    // it is common to leave text in the search fields, then open and close nodes
    // when the editor is clicked on again, recondition the tree to have the
    // last search result
    searchTree(search.getValue(), &root);
}

int SymbolTree::searchTree(juce::String text, SymbolTreeItem* node)
{
    int hits = 0;
    
    for (int i = 0 ; i < node->getNumSubItems() ; i++) {
        SymbolTreeItem* item = static_cast<SymbolTreeItem*>(node->getSubItem(i));

        if (item->getNumSubItems() > 0) {
            // this is an interior node, don't match on those but descend
        }
        else if (text.length() == 0) {
            // the search box was cleared, deselect the items to color them normally
            // and unhide them
            // if you bump the hit count it will keep the parent open
            //hits++;
            if (item->isSelected())
              item->setSelected(false, false, juce::NotificationType::sendNotification);
            item->setHidden(false);
        }
        else if (item->getName().containsIgnoreCase(text)) {
            hits++;
            // no, do not select them, they have to click to select
            //if (!item->isSelected())
            //item->setSelected(true, false, juce::NotificationType::sendNotification);
            item->setHidden(false);
        }
        else {
            // deselect if previously selected
            if (item->isSelected())
              item->setSelected(false, false, juce::NotificationType::sendNotification);
            item->setHidden(true);
        }

        hits += searchTree(text, item);
    }

    if (node != &root && node->getNumSubItems() > 0) {
        if (hits > 0) {
            node->setOpen(true);
            node->setHidden(false);
        }
        else {
            // nothing inside this node 
            node->setOpen(false);
            if (text.length() == 0) {
                // no specific search token, make sure it becomes visible again
                // for next time
                node->setHidden(false);
            }
            else {
                // nothing inside it and they typed something in, hide it
                // unless it is one of the top level ones
                if (node->getParentItem() != &root)
                  node->setHidden(true);
            }
        }
    }
    
    return hits;
}

void SymbolTree::endSearch()
{
    // formerly unhid things so they could browse normally
    // that isn't what you want if after search you want to click
    // on things and add favorites, make them clear the search box
    //unhide(&root);
}

//////////////////////////////////////////////////////////////////////
//
// Favorites
//
//////////////////////////////////////////////////////////////////////

juce::String SymbolTree::getFavorites()
{
    return favorites.joinIntoString(",");
}

void SymbolTree::addFavorite(juce::String name)
{
    favorites.add(name);

    SymbolTreeItem* parent = root.internChild("Favorites");
    SymbolTreeItem* neu = new SymbolTreeItem(name);
    SymbolTreeComparator comparator;
    parent->addSubItemSorted(comparator, neu);

    // auto open?
    parent->setOpen(true);
}

void SymbolTree::removeFavorite(juce::String name)
{
    favorites.removeString(name);
    
    SymbolTreeItem* parent = root.internChild("Favorites");
    parent->remove(name);
}

//////////////////////////////////////////////////////////////////////
//
// Item
//
//////////////////////////////////////////////////////////////////////

SymbolTreeItem::SymbolTreeItem()
{
}

SymbolTreeItem::SymbolTreeItem(juce::String s)
{
    name = s;
}

SymbolTreeItem::~SymbolTreeItem()
{
}

void SymbolTreeItem::setName(juce::String s)
{
    name = s;
}

juce::String SymbolTreeItem::getName()
{
    return name;
}

/**
 * Set this only if you want this to be a draggable tree.
 */
void SymbolTreeItem::setDragDescription(juce::String s)
{
    dragDescription;
}

/**
 * This is the Juce overload to return the drag desription if you want
 * this to be draggable.
 * If the dragDescription is empty then this should return void
 */
juce::var SymbolTreeItem::getDragSourceDescription()
{
    if (dragDesription.length > 0)
      return dragDescrpition;
    else
      return juce::var();
}

void SymbolTreeItem::setSymbol(Symbol* s)
{
    symbol = s;
}

Symbol* SymbolTreeItem::getSymbol()
{
    return symbol;
}

void SymbolTreeItem::addSymbol(Symbol* s)
{
    symbols.add(s);
}

juce::Array<class Symbol*>& getSymbols()
{
    return symbols;
}

void SymbolTreeItem::setAnnotation(juce::String s)
{
    annotation = s;
}

juce::String SymbolTreeItem::getAnnotation()
{
    return annotation;
}

void SymbolTreeItem::setColor(juce::Colour c)
{
    color = c;
}

juce::Colour SymbolTreeItem::getColor()
{
    return color;
}

bool SymbolTreeItem::isHidden()
{
    return hidden;
}

void SymbolTreeItem::setHidden(bool b)
{
    hidden = b;
}

void SymbolTreeItem::setNoSelect(bool b)
{
    noSelect = b;
}

//
// Tree Building
//

SymbolTreeItem* SymbolTreeItem::internChild(juce::String childName)
{
    SymbolTreeItem* found = nullptr;
    
    for (int i = 0 ; i < getNumSubItems() ; i++) {
        SymbolTreeItem* child = static_cast<SymbolTreeItem*>(getSubItem(i));
        if (child->getName() == childName) {
            found = child;
            break;
        }
    }

    if (found == nullptr) {
        found = new SymbolTreeItem(childName);
        found->setNoSelect(true);
        addSubItem(found);
    }

    return found;
}

void SymbolTreeItem::remove(juce::String childName)
{
    int index = -1;
    for (int i = 0 ; i < getNumSubItems() ; i++) {
        SymbolTreeItem* item = static_cast<SymbolTreeItem*>(getSubItem(i));
        if (item->name == childName) {
            index = i;
            break;
        }
    }
    if (index >= 0)
      removeSubItem(index, true);
}

//
// Juce Overrides
//

bool SymbolTreeItem::mightContainSubItems()
{
    return getNumSubItems() != 0;
}

int SymbolTreeItem::getItemHeight() const
{
    return (hidden) ? 0 : 14;
}

bool SymbolTreeItem::canBeSelected() const
{
    return !noSelect;
}

void SymbolTreeItem::paintItem(juce::Graphics& g, int width, int height)
{
    if (!hidden) {
        //g.fillAll(juce::Colours::grey);
        if (isSelected())
          g.setColour(juce::Colours::cyan);

        else {
            // if there is no user supplied color, make unselectables
            // look different
            if (color != juce::Colour(0))
              g.setColour(color);
            else if (noSelect)
              g.setColour(juce::Colours::yellow);
            else
              g.setColour(juce::Colours::white);
        }
        
        g.drawText(name, 0, 0, width, height, juce::Justification::left);
    }
}

/**
 * Right click handler for "Favorites".
 * An older experimental features, needs work.
 */
void SymbolTreeItem::itemClicked(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) {
        SymbolTree* tree = static_cast<SymbolTree*>(getOwnerView()->getParentComponent());
        juce::PopupMenu menu;
        juce::PopupMenu::Item item ("Favorite");
        item.setID(1);
        if (tree->favorites.contains(name))
          item.setTicked(true);
        menu.addItem(item);
        juce::PopupMenu::Options options;
        menu.showMenuAsync(options, [this] (int result) {popupSelection(result);});
    }
    else {
        SymbolTree* tree = static_cast<SymbolTree*>(getOwnerView()->getParentComponent());
        tree->itemClicked(this);
    }
}

/**
 * Menu handler for "Favorites"
 * An older experimental features, needs work.
 */
void SymbolTreeItem::popupSelection(int result)
{
    if (result == 1) {
        SymbolTree* tree = static_cast<SymbolTree*>(getOwnerView()->getParentComponent());
        if (tree->favorites.contains(name)) {
            tree->removeFavorite(name);
        }
        else {
            tree->addFavorite(name);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Sort Comparator
//
//////////////////////////////////////////////////////////////////////

int SymbolTreeComparator::compareElements(juce::TreeViewItem* first, juce::TreeViewItem* second)
{
    juce::String name1 = (static_cast<SymbolTreeItem*>(first))->getName();
    juce::String name2 = (static_cast<SymbolTreeItem*>(second))->getName();

    return name1.compareIgnoreCase(name2);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
