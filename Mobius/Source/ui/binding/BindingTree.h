
#pragma once

#include <JuceHeader.h>

#include "../../model/Symbol.h"

#include "SymbolTree.h"

class BindingTree : public SymbolTree
{
  public:

    BindingTree();
    ~BindingTree();

    void initialize(class Provider* p);
    void selectFirst();

  private:

};

// this is used by the original dynamic tree builder which is no longer used
class ParameterTreeComparator
{
  public:

    ParameterTreeComparator(class TreeForm* tf);

    int compareElements(juce::TreeViewItem* first, juce::TreeViewItem* second);

  private:

    class TreeForm* form = nullptr;
};

