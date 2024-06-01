
#pragma once

#include "UIParameterIds.h"

class UIParameterHandler
{
  public:
    void get(UIParameterId id, void* obj, class ExValue* value);
    void set(UIParameterId id, void* obj, class ExValue* value);
};

