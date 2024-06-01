/**
 * A field grid is a set of Fields that can be arranged in columns
 * The grid owns the Fields which are deleted when the grid is deleted.
 * The grid supports auto sizing to become as large as necessary to contain
 * the columns and fields.
 *
 * TODO: should the column split be a property of the field or the grid?
 */

#pragma once

#include <JuceHeader.h>
#include "Field.h"

class FieldGrid : public juce::Component {

  public:
    
    FieldGrid();
    ~FieldGrid();
    
    void add(Field* field, int column = 0);

    // add our contained Fields to the array
    void gatherFields(juce::Array<Field*>& fields);

    void render();
    juce::Rectangle<int> getMinimumSize();
    
    //
    // Juce interface
    //

    void resized() override;
    void paint (juce::Graphics& g) override;
    
  private:

    juce::OwnedArray<juce::OwnedArray<Field>> columns;

};
