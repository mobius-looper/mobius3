
#pragma once

#include "../ui/BasePanel.h"    

class SymbolTableContent : public juce::Component, public juce::TableListBoxModel
{
  public:

    SymbolTableContent();
    ~SymbolTableContent();

    void prepare();
    void resized() override;

    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber,
                            int /*width*/, int /*height*/,
                            bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
                   int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

  private:

    juce::Array<class Symbol*> symbols;

    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};

    void initTable();
    void initColumns();
    juce::String getCellText(int row, int columnId);

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

