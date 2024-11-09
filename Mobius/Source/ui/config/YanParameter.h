/**
 * YanForm field for editing Symbol parameters.
 *
 * This is similar to the old Field.h, it can take on several representations
 * appropriate for the parameter definition.  It reads and writes values to a ValueSet
 *
 */

#include <JuceHeader.h>

#include "../common/YanField.h"

class YanParameter : public YanField
{
  public:

    YanParameter(juce::String label);
    ~YanParameter();

    void init(class Symbol* s);
    void load(class ValueSet* set);
    void save(class ValueSet* set);

    int getPreferredComponentWidth() override;
    void resized() override;

    Symbol* getSymbol() {
        return symbol;
    }

    void load(class MslValue* v);
    void save(class MslValue* v);

  private:

    Symbol* symbol = nullptr;
    bool isText = false;
    bool isCombo = false;
    bool isCheckbox = false;
    
    // various renderings
    YanCombo combo {""};
    YanInput input {""};
    YanCheckbox checkbox {""};
    
};

