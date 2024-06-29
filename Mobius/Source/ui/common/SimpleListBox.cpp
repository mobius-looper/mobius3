#include <JuceHeader.h>

#include "SimpleListBox.h"

SimpleListBox::SimpleListBox()
{
    setName("SimpleListBox");
    
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
    addAndMakeVisible(listBox);

    // don't need to do this, the default is reasonable
    //juce::Font font = juce::Font(juce::Font (16.0f, juce::Font::bold));
    //listBox.setRowHeight (font.getHeight());
    
    listBox.setModel (this);   // Tell the listbox where to get its data model
    listBox.setColour (juce::ListBox::textColourId, juce::Colours::black);
    listBox.setColour (juce::ListBox::backgroundColourId, juce::Colours::white);

    listBox.setMultipleSelectionEnabled(true);
    listBox.setClickingTogglesRowSelection(true);
}

SimpleListBox::~SimpleListBox()
{
}

void SimpleListBox::setValues(juce::StringArray& src)
{
    // this copies
    values = src;

    // docs say "This must only be called from the main message thread"
    // not sure what that means
    listBox.updateContent();
}

void SimpleListBox::setValueLabels(juce::StringArray& src)
{
    valueLabels = src;
    listBox.updateContent();
}

void SimpleListBox::add(juce::String label)
{
    values.add(label);
    listBox.updateContent();
}

/**
 * Sorting is only partially implemented.  It only sorts the values list
 * and does not attempt to keep selection indexes and the alternate label
 * list in sync.  It can only be used for things that have simple value
 * lists that sort immediately after populating the allowed values and
 * before user interaction.
 */
void SimpleListBox::sort()
{
    // argument is ignoreCase
    values.sort(false);
}

void SimpleListBox::clear()
{
    values.clear();
    valueLabels.clear();
    listBox.updateContent();
}

/**
 * Set the initial selected rows.
 */
void SimpleListBox::setSelectedValues(juce::StringArray& selected)
{
    listBox.deselectAllRows();
    for (int i = 0 ; i < selected.size() ; i++) {
        juce::String value = selected[i];
        int index = values.indexOf(value);
        if (index >= 0) {
            listBox.selectRow(index,
                              true /* don't scroll */,
                              false /* delect others FIRST */);
        }
    }
}

/**
 * Return only the selected values.
 */
void SimpleListBox::getSelectedValues(juce::StringArray& selected)
{
    juce::SparseSet<int> rows = listBox.getSelectedRows();
    for (int i = 0 ; i < rows.size() ; i++) {
        int row = rows[i];
        selected.add(values[row]);
    }
}

int SimpleListBox::getSelectedRow()
{
    return listBox.getSelectedRow();
}

void SimpleListBox::setSelectedRow(int index)
{
    // dontScrollToShowThisRow=false, deselectOthersFirst=true
    listBox.selectRow(index);
    // didn't seem to force a repaint?
    listBox.repaint();
}

juce::String SimpleListBox::getSelectedValue()
{
    juce::String value;
    int row = getSelectedRow();
    if (row >= 0)
      value = values[row];
    return value;
}

void SimpleListBox::deselectAll()
{
    listBox.deselectAllRows();
}

void SimpleListBox::paint(juce::Graphics& g)
{
    /* This demo code just fills the component's background and
       draws some placeholder text to get you started.

       You should replace everything in this method with your own
       drawing code..
    */
	// g.fillAll (juce::Colours::lightgrey);   // clear the background
	g.fillAll (juce::Colours::blue);   // clear the background
    
}

void SimpleListBox::resized()
{
    // inner ListBox fills us, makes us whole
	listBox.setBounds(getLocalBounds());
}

//
// ListBoxModel
//

int SimpleListBox::getNumRows()
{
    return values.size();
}

void SimpleListBox::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                      int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
      g.fillAll (juce::Colours::lightblue);        

    g.setColour (juce::Colours::black);
    g.setFont (height * 0.7f);
   
    // g.drawText ("Row Number " + String (rowNumber + 1), 5, 0, width, height,
    // Justification::centredLeft, true);
    juce::String s;
    if (valueLabels.size() > 0)
      s = valueLabels[rowNumber];
    else
      s = values[rowNumber];

    g.drawText (s, 5, 0, width, height, juce::Justification::centredLeft, true);
}

/**
 * This is called when the user clicks on a row AND when it is set
 * programatically.  If you only want to see manual selection
 * the the Listener should pay attention to listBoxItemClicked instead.
 */
void SimpleListBox::selectedRowsChanged (int lastRowSelected)
{
    if (listener != nullptr)
      listener->selectedRowsChanged(this, lastRowSelected);
}

/**
 * This is called when the user clicks on a row AFTER calling
 * selectedRows changed.  If you only want to know about manual selections,
 * use this listener.
 */
void SimpleListBox::listBoxItemClicked(int row, const juce::MouseEvent& event)
{
    (void)event;
    if (listener != nullptr)
      listener->listBoxItemClicked(this, row);
}

/**
 * If we have labels need to decide what to return
 * or provide an interface to access both.
 */
juce::String SimpleListBox::getRowValue(int index)
{
    return values[index];
}
