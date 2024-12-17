/**
 * A model given to the MslContext from an MSL script that describes
 * a form for the containing application to display to gather values.
 * This is built at runtime from a form definition specified with the
 * MslFormNode and mslFieldNodes.
 *
 * The container is expected to asynchrousnly build and a suitable form
 * using any UI technology.
 * 
 */

#pragma once

class MslForm
{
  public:

    class Field {
      public:

        typedef enum {
            TypeString,
            TypeInt,
            TypeBool
        } Type;

        // variable type to influence rendering
        Type type = TypeString;

        // label to put in front of the field
        juce::String label;

        // MSL variable to read and write
        juce::String variable;

        // for string and number fields, a suggested width in characters
        int charWidth = 0;

        // for numeric fields, an optional range
        int low = 0;
        int high = 0;

        // for string fields, a list of allowed values that could be
        // rendered as a combo box
        juce::StringArray allowedValues;

        // the initial value for the field on the way out, and the final
        // value on the way back in
        MslValue value;
    };

    // text intended for display above the form, such as the title bar
    // of a popup window
    juce::String title;

    // the fields in the form
    juce::OwnedArray<Field> fields;

    // close button names
    // There will usually be a single "Ok" button
};

