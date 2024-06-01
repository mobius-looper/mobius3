/*
 * Provides a basic headered, scrolling table.
 *
 */

#pragma once

#include <JuceHeader.h>
#include "JLabel.h"

class SimpleTable : public juce::Component, public juce::TableListBoxModel
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void tableTouched(SimpleTable* t) = 0;
    };
    
    SimpleTable();
    ~SimpleTable();

    void addListener(Listener* l) {
        listener = l;
    }

    // Height in pixels of the header
    void setHeaderHeight(int height);
    int getHeaderHeight();
    
    // Height in pixels of the row, defaults to 22
    void setRowHeight(int height);
    int getRowHeight();

    // names of the columns with some presentation defaults
    void setColumnTitles(juce::StringArray& src);

    // this would be initial width if we allow resizing columns
    // or it could set the width after resizing
    void setColumnWidth(int col, int width);

    void setCell(int row, int col, juce::String text);
    void dumpCells();
    void clear();
    void updateContent();
    
    int getSelectedRow();
    void setSelectedRow(int row);
    int getSelectedColumn();
    
    // doesn't seem to work to change this after the fact?
    // Constructor set it to true but I couldn't set it false in ButtonPanel
    void setMultipleSelectionEnabled(bool b) {
        table.setMultipleSelectionEnabled(b);
    }

    // juce::Component overrides
    
    void resized() override;
    void paint (juce::Graphics& g) override;

    int getNumRows() override;
    void paintRowBackground (juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected) override;
    void paintCell (juce::Graphics& g, int rowNumber, int columnId,
                    int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

    // int getColumnAutoSizeWidth (int columnId) override

  private:
    
    Listener* listener = nullptr;

    // juce::Array<juce::StringArray> columns;
    juce::OwnedArray<juce::StringArray> columns;

    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleTable)
};
