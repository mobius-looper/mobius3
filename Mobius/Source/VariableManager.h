/**
 * Class encapsulating management of VariableDefinitions
 */

#pragma once

#include <JuceHeader.h>
#include "model/VariableDefinition.h"

class VariableManager
{
  public:

    VariableManager(class Provider* p);
    ~VariableManager();

    void install();

    VariableDefinitionSet* getVariables() {
        return &variables;
    }

  private:

    class Provider* provider = nullptr;
    VariableDefinitionSet variables;

};

    
