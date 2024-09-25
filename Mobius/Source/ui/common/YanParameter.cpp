
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"

#include "YanField.h"

#include "YanParameter.h"

YanParameter::YanParameter(juce::String label) : YanField(label)
{
}

YanParameter::~YanParameter()
{
}

void YanParameter::init(Symbol* s)
{
    symbol = s;
    isText = false;
    isCombo = false;

    if (s == nullptr) {
        Trace(1, "YanParameter: Missing symbol");
    }
    else {
        ParameterProperties* props = s->parameterProperties.get();
        if (props == nullptr) {
            Trace(1, "YanParameter: Symbol is not associated with a parameter %s", s->getName());
        }

        // TypeEnum doesn't seem to be set reliably, look for a value list
        //nelse if (props->type == TypeEnum) {
        else if (props->type == TypeEnum) {
            isCombo = true;
            addAndMakeVisible(&combo);

            combo.setItems(props->values);
        }
        else {
            isText = true;
            addAndMakeVisible(&input);
        }
    }
}

int YanParameter::getPreferredWidth()
{
    int width = 0;
    if (isCombo)
      width = combo.getPreferredWidth();
    else
      width = input.getPreferredWidth();
    return width;
}

void YanParameter::resized()
{
    if (isCombo)
      combo.setBounds(getLocalBounds());
    else
      input.setBounds(getLocalBounds());
}
