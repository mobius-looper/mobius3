
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

    // for drag-and-drop
    juce::var getDragSourceDescription();

    // kludge for tree forms, figure out how this should work
    void setAnnotation(juce::String s) {
        annotation = s;
    }
    juce::String getAnnotation() {
        return annotation;
    }

    void setSymbolName(juce::String s) {
        symbolName = s;
    }
    juce::String getSymbolName() {
        return symbolName;
    }

    void addSymbol(class Symbol* s);
    juce::Array<class Symbol*>& getSymbols() {
        return symbols;
    }
    void setColor(juce::Colour c);
    juce::Colour getColor();
    
    // TreeViewItem
    
	bool mightContainSubItems() override;
	void paintItem(juce::Graphics& g, int width, int height) override;
    int getItemHeight() const override;
    bool canBeSelected() const override;
    void itemClicked(const juce::MouseEvent& e) override;
    
    // Extensions
    
    void setName(juce::String s);
    juce::String getName();
    bool isHidden();
    void setHidden(bool b);
    void setNoSelect(bool b);
    SymbolTreeItem* internChild(juce::String name);
    void remove(juce::String childName);
    void popupSelection(int result);
    
  private:
    
    juce::String name;
    juce::String annotation;
    juce::String symbolName;
    bool hidden = false;
    bool noSelect = false;
    juce::Colour color;

    // for interior nodes, the symbols under it
    // used for the session editor
    juce::Array<class Symbol*> symbols;
    
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

    void disableSearch();
    void setListener(Listener* l);
    void setDropListener(DropTreeView::Listener* l);
    
    void resized() override;

    // various ways to load it
    void loadSymbols(class SymbolTable* table, juce::String favorites);
    void loadSymbols(class SymbolTable* table, juce::String favorites,
                     juce::String includes);
    
    
    void addFavorite(juce::String name);
    void removeFavorite(juce::String name);
    juce::String getFavorites();

    void inputEditorShown(YanInput* input) override;
    void inputEditorChanged(YanInput* input, juce::String text) override;
    void inputEditorHidden(YanInput* input) override;

    void itemClicked(SymbolTreeItem* item);

    juce::StringArray favorites;

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
    
    void startSearch();
    int searchTree(juce::String text, SymbolTreeItem* node);
    void endSearch();
    void unhide(SymbolTreeItem* node);

    
};
