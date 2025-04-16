
#include <JuceHeader.h>

#include "YanField.h"
#include "YanListBox.h"

YanListBox::YanListBox(juce::String label) : YanField(label)
{
    listbox.setModel(this);
    addAndMakeVisible(listbox);
}

YanListBox::~YanListBox()
{
}

void YanListBox::setListener(YanListBox::Listener* l)
{
    listener = l;
}

void YanListBox::setItems(juce::StringArray names)
{
    // todo: detect if this was already added
    items.addArray(names);
    listbox.updateContent();
}

int YanListBox::getPreferredComponentWidth()
{
    return 300;
}

int YanListBox::getPreferredComponentHeight()
{
    return 200;
}

void YanListBox::setSelection(int index)
{
    (void)index;
}

int YanListBox::getSelection()
{
    return 0;
}

void YanListBox::resized()
{
    listbox.setBounds(getLocalBounds());
}

int YanListBox::getNumRows()
{
    return items.size();
}

juce::String YanListBox::getNameForRow(int row)
{
    juce::String name;
    if (row >= 0 && row < items.size())
      name = items[row];
    return name;
}

void YanListBox::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                   int width, int height, bool rowIsSelected)
{
    (void)rowIsSelected;
    //if (rowIsSelected)
    //g.fillAll (juce::Colours::lightblue);

    g.setColour (juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont ((float) height * 0.7f);
    g.drawText (getNameForRow(rowNumber), 5, 0, width, height,
                juce::Justification::centredLeft, true);
}
