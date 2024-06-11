
#pragma once

#include "../ui/BasePanel.h"    
#include "../ui/common/BasicTable.h"

class SymbolTableContent : public juce::Component, public BasicTable::Model
{
  public:

    SymbolTableContent();
    ~SymbolTableContent();

    void prepare();
    void resized() override;

    // BasicTable::Model
    int getNumRows() override;
    juce::String getCellText(int row, int columnId);

  private:

    juce::Array<class Symbol*> symbols;
    BasicTable table;

};

class SymbolTablePanel : public BasePanel
{
  public:

    SymbolTablePanel() {
        setTitle("Symbols");
        setContent(&content);
        setSize(800, 600);
    }
    ~SymbolTablePanel() {}

    void showing() override {
        content.prepare();
    }

  private:

    SymbolTableContent content;
};

