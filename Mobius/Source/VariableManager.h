/**
 * Class encapsulating management of VariableDefinitions
 */

#pragma once

#include <JuceHeader.h>
#include "model/VariableDefinition.h"

class VariableManager
{
  public:

    VariableManager(Supervisor* super);
    ~VariableManager();

    void install();

    VariableDefinitionSet* getVariables() {
        return &variables;
    }

  private:

    class Supervisor* supervisor = nullptr;
    VariableDefinitionSet variables;

};

    
