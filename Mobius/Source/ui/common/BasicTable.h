/**
 * Extension of TableListBox that has the common options I use.
 */

#pragma once

#include <JuceHeader.h>

class BasicTable : public juce::TableListBox, public juce::TableListBoxModel
{
  public:

    /**
     * Two ways you can inject cell renering.  1) extend BasicTable
     * and override getNumRows and getCellText or 2) implement
     * the Model and set it on the table.
     */
    class Model {
      public:
        Model() {};
        virtual ~Model() {}

        virtual int getNumRows() = 0;
        virtual juce::String getCellText(int row, int columnId) = 0;
        virtual bool getCellCheck(int row, int columnId) {
            (void)row;
            (void)columnId;
            return false;
        }
        virtual void setCellCheck(int row, int columnId, bool state) {
            (void)row;
            (void)columnId;
            (void)state;
        }
        virtual juce::Colour getCellColor(int row, int columnId) {
            (void)row;
            (void)columnId;
            return juce::Colour(0);
        }
    };

    BasicTable();
    ~BasicTable() {}

    void addColumn(juce::String name, int id, int width = 150);
    void addColumnCheckbox(juce::String name, int id);

    void setBasicModel(Model* m);
    // subclass can override these to insert the model as an alternative
    // to using setBasicModel
    virtual int getNumRows() override;
    virtual juce::String getCellText(int row, int columnId);

    // BasicTableChedkbox callbacks
    bool getCheck(int row, int column);
    void doCheck(int row, int column, bool state);

    // TableListBoxModel
    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
                   int width, int height, bool rowIsSelected) override;
    void paintRowBackground(juce::Graphics& g, int rowNumber,
                            int width, int height,
                            bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

    Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                        Component* existingComponentToUpdate) override;
    
    
  private:

    Model* model = nullptr;

    juce::Array<bool> checkboxColumns;

    void setCheckboxFlag(int columnId, bool flag);
    bool needsCheckbox(int row, int col);

};

/**
 * Custom table cell component for a checkbox.
 */
class BasicTableCheckbox : public juce::Component
{
  public:
    
    BasicTableCheckbox(BasicTable& table) : owner(table)
    {
        addAndMakeVisible(checkbox);
        checkbox.onClick = [this] {owner.doCheck(row, column, (int)(checkbox.getToggleState()));};
    }
    ~BasicTableCheckbox() {}

    void resized() {
        // this is what the tutorial does
        // "position the component within its parent, leaving the specified
        //  number of pixels around each edge"
        //checkbox.setBoundsInset(juce::BorderSize<int>(2));
        
        // hack for centering
        juce::Rectangle<int> area = getLocalBounds();
        int checkWidth = area.getHeight() - 4;
        int centerLeft = (area.getWidth() / 2) - (checkWidth / 2);
        // getting the right edge of the checkbox border clipped, make it bigger
        checkbox.setBounds(centerLeft, 2, checkWidth + 2, checkWidth);
    }
    
    void setRowAndColumn (const int newRow, const int newColumn) {
        row = newRow;
        column = newColumn;
        checkbox.setToggleState(owner.getCheck(row, column), juce::dontSendNotification);
    }
    
  private:
    
    BasicTable& owner;
    juce::ToggleButton checkbox;
    int row = 0;
    int column = 0;
};
    
