/**
 * Wraps the MslVariableNode for use in Linkages
 */

#pragma once

#include "MslModel.h"

class MslVariableExport
{
  public:

    MslVariableExport() {}
    ~MslVariableExport() {}

    juce::String getName() {
        return (node != nullptr) ? node->name : "";
    }

  protected:
    
    class MslVariable* getNode() {
        return (node != nullptr) ? node.get() : nullptr;
    }

    void setNode(MslVariable* v) {
        node.reset(v);
    }

  private:
    
    // the parse tree for the variable node
    std::unique_ptr<class MslVariable> node;

};


    
