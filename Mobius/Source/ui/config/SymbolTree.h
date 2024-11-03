
#pragma once

#include <JuceHeader.h>

#include "../common/YanField.h"

class SymbolTreeItem : public juce::TreeViewItem
{
  public:
    
	SymbolTreeItem();
	SymbolTreeItem(juce::String s);
	~SymbolTreeItem();

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
    bool hidden = false;
    bool noSelect = false;
    
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

    void resized() override;

    void loadSymbols(class SymbolTable* table, juce::String favorites);
    void addFavorite(juce::String name);
    void removeFavorite(juce::String name);
    juce::String getFavorites();

    void inputEditorShown(YanInput* input) override;
    void inputEditorChanged(YanInput* input, juce::String text) override;
    void inputEditorHidden(YanInput* input) override;

    void itemClicked(SymbolTreeItem* item);

    juce::StringArray favorites;
    
  private:

    juce::TreeView tree;
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
