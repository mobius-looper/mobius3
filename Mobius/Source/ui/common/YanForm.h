/**
 * Yet another form structure.
 */

#pragma once

#include <JuceHeader.h>

class YanForm : public juce::Component
{
  public:

    // the default row height
    // todo: force this consistently or let fields define their own height?
    static const int RowHeight = 20;

    YanForm() {}
    ~YanForm() {}

    // form rendering properties
    void setTopInset(int size);
    void setLabelCharWidth(int chars);
    void setLabelColor(juce::Colour color);
    void setFillWidth(bool b);
    
    // fields
    void add(class YanField* f);

    // rendering
    int getPreferredHeight();
    int getPreferredWidth();

    // Juce 
    void resized() override;
    
  private:

    // fields arranged in a single column
    // fields are normally static objects owned by the container of the form
    juce::Array<class YanField*> fields;

    // generated labels for the fields, think about just using paint() for this
    juce::OwnedArray<juce::Label> labels;

    int topInset = 0;
    int labelCharWidth = 0;
    juce::Colour labelColor;
    bool fillWidth = false;
    
    int getLabelAreaWidth();
    int getFieldAreaWidth();
    
};
