
#pragma once

#include <JuceHeader.h>

#include "../common/YanField.h"
#include "DropTreeView.h"

class SymbolTreeItem : public juce::TreeViewItem
{
  public:
    
	SymbolTreeItem();
	SymbolTreeItem(juce::String s);
	~SymbolTreeItem();

    void setName(juce::String s);
    juce::String getName();

    void setDragDescription(juce::String s);
    
    void setSymbol(class Symbol* s);
    Symbol* getSymbol();
    void addSymbol(class Symbol* s);
    juce::Array<class Symbol*>& getSymbols();

    void setAnnotation(juce::String s);
    juce::String getAnnotation();

    bool isHidden();
    void setHidden(bool b);
    void setNoSelect(bool b);
    void setColor(juce::Colour c);
    juce::Colour getColor();
    
    SymbolTreeItem* internChild(juce::String name);
    void remove(juce::String childName);
    
    // TreeViewItem overloads
    
	bool mightContainSubItems() override;
	void paintItem(juce::Graphics& g, int width, int height) override;
    int getItemHeight() const override;
    bool canBeSelected() const override;
    void itemClicked(const juce::MouseEvent& e) override;
    juce::var getDragSourceDescription() override;

    // Favorites experiment
    void popupSelection(int result);
    
  private:

    // the node name displayed in the UI
    // for leaf nodes, this will usually be the displayName of the Symbol
    // but may be abbreviated
    // for interior nodes, this will be the category name
    juce::String name;

    // when used by the Session editor, the name of the form to display
    // when this node is clicked
    // todo: rename this "formName" to make it clear what it is
    juce::String annotation;

    // if the tree supports dragging out, set this to a non-empty string
    juce::String dragDescription;

    // for leaf nodes the Symbol the item is associated with
    Symbol* symbol = nullptr;

    // for interior nodes, all of the symbols that fit within this category
    // todo: is this really necessary?
    juce::Array<class Symbol*> symbols;
    
    // various display optiosn 
    bool hidden = false;
    bool noSelect = false;
    juce::Colour color;
};

class SymbolTreeComparator
{
  public:

    int compareElements(juce::TreeViewItem* first, juce::TreeViewItem* second);
};


class SymbolTree : public juce::Component, public YanInput::Listener
{
  public:

    SymbolTree();
    ~SymbolTree();

    class LookAndFeel : public juce::LookAndFeel_V4 {
      public:
        LookAndFeel(SymbolTree* st);

        void drawTreeviewPlusMinusBox (juce::Graphics& g,
                                       const juce::Rectangle<float>& area,
                                       juce::Colour backgroundColour,
                                       bool isOpen, bool isMouseOver) override;
      private:
        SymbolTree* symbolTree = nullptr;
    };

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void symbolTreeClicked(SymbolTreeItem* item) =  0;
    };

    void setListener(Listener* l);
    void setDropListener(DropTreeView::Listener* l);
    void itemClicked(SymbolTreeItem* item);
    
    // old load interface, remove
    void loadSymbols(class SymbolTable* table, juce::String favorites);
    void loadSymbols(class SymbolTable* table, juce::String favorites,
                     juce::String includes);

    // wandering
    SymbolTreeItem* findAnnotatedItem(juce::String annotation);
    
    // favorites
    void addFavorite(juce::String name);
    void removeFavorite(juce::String name);
    juce::String getFavorites();
    juce::StringArray favorites;

    // search
    void disableSearch();
    void inputEditorShown(YanInput* input) override;
    void inputEditorChanged(YanInput* input, juce::String text) override;
    void inputEditorHidden(YanInput* input) override;

    void resized() override;

  protected:

    LookAndFeel laf {this};
    Listener* listener = nullptr;
    bool searchDisabled = false;

    // use this just to get drop target
    //juce::TreeView tree;
    DropTreeView tree;
    SymbolTreeItem root;
    YanInput search {"Search"};

    juce::HashMap<juce::String,SymbolTreeItem*> items;

    SymbolTreeItem* internPath(SymbolTreeItem* parent, juce::StringArray path);
    juce::StringArray parsePath(juce::String s);
    SymbolTreeItem* findAnnotatedItem(SymbolTreeItem* parent, juce::String annotation);
    
    void startSearch();
    int searchTree(juce::String text, SymbolTreeItem* node);
    void endSearch();
    void unhide(SymbolTreeItem* node);
    
    
};
