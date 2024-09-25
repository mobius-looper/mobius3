/**
 * YanForm field for editing Symbol parameters.
 *
 * This is similar to the old Field.h, it can take on several representations
 * appropriate for the parameter definition.  It reads and writes values to a ValueSet
 *
 */

#include <JuceHeader.h>

class YanParameter : public YanField
{
  public:

    YanParameter(juce::String label);
    ~YanParameter();

    void init(class Symbol* s);
    void load(class ValueSet* set);
    void save(class ValueSet* set);

    int getPreferredWidth() override;
    void resized() override;
    
  private:

    Symbol* symbol = nullptr;
    bool isText = false;
    bool isCombo = false;
    
    // various renderings
    YanCombo combo {""};
    YanInput input {""};
    
};

