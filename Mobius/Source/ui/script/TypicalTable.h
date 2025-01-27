
#pragma once

#include "../common/ButtonBar.h"

class TypicalTable : public juce::Component,
                     public juce::TableListBoxModel,
                     public ButtonBar::Listener
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        // this happens when you click on a row
        virtual void typicalTableRowClicked(TypicalTable* t, int row) {(void)t; (void)row;}
        // this happens when you click on empty space
        virtual void typicalTableSpaceClicked(TypicalTable* t) {(void)t;}
        // this happens when you click or move with the keyboard
        virtual void typicalTableChanged(TypicalTable* t, int row) {(void)t; (void)row;}
    };
    void setListener(Listener* l) {
        listener = l;
    }

    TypicalTable();
    ~TypicalTable();

    int getSelectedRow();
    void selectRow(int r);
    
    // defer most initialization till the subclass has control
    void initialize();

    // subclass overloads
    virtual int getRowCount() = 0;
    virtual juce::String getCellText(int rowNumber, int columnId) = 0;
    virtual void doCommand(juce::String name) {}

    void addColumn(juce::String name, int id, int width);
    void addCommand(juce::String name);
    void updateContent();
    
    int getPreferredHeight();
    int getPreferredWidth();
    
    // ButtonBar::Listener
    void buttonClicked(juce::String name) override;

    // Component
    void resized() override;
    
    // TableListBoxModel
    
    int getNumRows() override;
    void paintRowBackground (juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected) override;
    void paintCell (juce::Graphics& g, int rowNumber, int columnId,
                    int width, int height, bool rowIsSelected) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

  protected:
    
    Listener* listener = nullptr;
    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};

    bool paintDropTarget = false;
    int dropTargetRow = -1;

  private:
    
    bool hasCommands = false;
    ButtonBar commands;

    void initTable();

};

