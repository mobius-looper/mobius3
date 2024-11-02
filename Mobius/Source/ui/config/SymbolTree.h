
#pragma once

#include <JuceHeader.h>

class SymbolTreeItem : public juce::TreeViewItem
{
  public:
    
	SymbolTreeItem();
	SymbolTreeItem(juce::String s);
	~SymbolTreeItem();

    // TreeViewItem
    
	bool mightContainSubItems() override;
	void paintItem(juce::Graphics& g, int width, int height) override;

    // Extensions
    
    void setName(juce::String s);
    juce::String getName();
    SymbolTreeItem* internChild(juce::String name);

  private:

    juce::String name;
    
};

class SymbolTree : public juce::Component
{
  public:

    SymbolTree();
    ~SymbolTree();

    void resized() override;

    void loadSymbols(class SymbolTable* table);

  private:

    juce::TreeView tree;
    SymbolTreeItem root;

    juce::HashMap<juce::String,SymbolTreeItem*> items;

    SymbolTreeItem* internPath(SymbolTreeItem* parent, juce::StringArray path);
    juce::StringArray parsePath(juce::String s);
    
};
