/**
 * See here for simple example
 *
 * https://forum.juce.com/t/treeview-tutorial/20187/6
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"

#include "SymbolTree.h"

//////////////////////////////////////////////////////////////////////
//
// Tree
//
//////////////////////////////////////////////////////////////////////

SymbolTree::SymbolTree()
{
    addAndMakeVisible(tree);
    tree.setRootItem(&root);
    tree.setRootItemVisible(false);
}

SymbolTree::~SymbolTree()
{
}

void SymbolTree::resized()
{
    tree.setBounds(getLocalBounds());
}

void SymbolTree::loadSymbols(SymbolTable* symbols)
{
    for (auto symbol : symbols->getSymbols()) {
        
        if (!symbol->hidden) {
        
            SymbolTreeItem* parent = nullptr;

            if (symbol->parameterProperties != nullptr)
              parent = root.internChild("Parameters");
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
            else
              parent = root.internChild("Other");

            if (parent != nullptr) {

                SymbolTreeItem* neu = new SymbolTreeItem(symbol->name);

                if (symbol->treePath == "") {
                    parent->addSubItem(neu);
                }
                else {
                    juce::StringArray path = parsePath(symbol->treePath);
                    SymbolTreeItem* deepest = internPath(parent, path);
                    deepest->addSubItem(neu);
                }
            }
        }
    }
}

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
    return juce::StringArray::fromTokens(s, "/");
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

bool SymbolTreeItem::mightContainSubItems()
{
    return getNumSubItems() != 0;
}

void SymbolTreeItem::paintItem(juce::Graphics& g, int width, int height)
{
    //g.fillAll(juce::Colours::grey);
    g.setColour(juce::Colours::white);
    g.drawText(name, 0, 0, width, height, juce::Justification::left);
}

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
        addSubItem(found);
    }

    return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
