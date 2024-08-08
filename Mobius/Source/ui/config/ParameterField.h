/**
 * A ParameterField extends the Field model to provide
 * initialization based on a Mobius Parameter definition.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/UIParameter.h"
#include "../common/Field.h"

class ParameterField : public Field
{
  public:

    ParameterField(class Supervisor* s, class UIParameter* p);
    ~ParameterField();

    static Field::Type convertParameterType(UIParameterType intype);

    UIParameter* getParameter() {
        return parameter;
    }
    
    void loadValue(void* sourceObject);
    void saveValue(void* targetObject);
    
  private:
    class Supervisor* supervisor = nullptr;
    UIParameter* parameter = nullptr;

};
