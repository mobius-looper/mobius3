/**
 * Yet another form structure.
 */

#pragma once

#include <JuceHeader.h>

#include "YanField.h"

/**
 * DragAndDropContainer is for when forms are used inside the Session or Parmaeter set
 * editor and fields are allowed to be removed from the form.
 */
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
    void addSpacer();
    YanField* find(juce::String label);

    // rendering
    int getPreferredHeight();
    int getPreferredWidth();

    // Juce 
    void resized() override;
    void forceResize();

    bool remove(YanField* f);
    
  private:

    // fields arranged in a single column unless adjacent
    // fields are normally static objects owned by the container of the form
    juce::Array<class YanField*> fields;

    // left labels are in a column with padding and justification
    juce::Array<juce::Label*> labels;

    YanSpacer spacer;

    int topInset = 0;
    int labelCharWidth = 0;
    juce::Colour labelColor;
    bool fillWidth = false;
    
    int getLabelAreaWidth();
    int getFieldAreaWidth();
    
};
