/**
 * YanForm field for editing ValueSet items.
 *
 * Loosly based on YanParameter but uses the Form/Field model
 * to define the form.
 *
 * It may take on several internal representations appropriate for the
 * parameter type.  It reads and writes values to a ValueSet.
 *
 * A Provider must be required on initialization in case the Parameter has
 * a displayHelper, typicall used for combo boxes that show names of structures
 * like ParameterSets or GroupDefinitions.
 *
 * todo: When the dust settles see if there is anything we can share between this
 * and YanParameter
 *
 */

#pragma once

#include <JuceHeader.h>

#include "YanField.h"

class ValueSetField : public YanField,
                      public YanCombo::Listener,
                      public YanInput::Listener
{
  public:

    ValueSetField(juce::String label);
    ~ValueSetField();

    void init(class Provider* p, class Field* def);

    int getPreferredComponentWidth() override;
    void resized() override;

    class Field* getDefinition() {
        return definition;
    }

    void load(class MslValue* v);
    void save(class MslValue* v);

    void yanComboSelected(YanCombo* c, int selection) override;
    void yanInputChanged(YanInput* i) override;

  private:
    
    typedef enum {
        TypeText,
        TypeCombo,
        TypeCheckbox,
        TypeFile
    } Type;
    
    void initCombo(class Provider* p, class Symbol* s);
    
    class Field* definition = nullptr;
    juce::StringArray structureNames;
    Type type = TypeText;
    
    // various renderings
    YanCombo combo {""};
    YanInput input {""};
    YanCheckbox checkbox {""};
    YanFile file {""};
    
    void initCombo(Provider* p, Field* def);
    
};

