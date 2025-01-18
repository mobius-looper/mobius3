/**
 * Extension of YanForm that assists with large forms of Symbol parameters.
 */

#include <JuceHeader.h>

#include "../../model/SymbolId.h"
#include "../common/YanForm.h"

class YanParameterForm : public YanForm
{
  public:
    YanParameterForm(class Provider* p);
    ~YanParameterForm();

    class YanParameter* addField(SymbolId id);

    void load(class ValueSet* set);
    void save(class ValueSet* set);
    
  private:

    class Provider* provider = nullptr;

    juce::Array<class YanParameter*> allFields;
    juce::OwnedArray<class YanParameter> ownedFields;
    
};

