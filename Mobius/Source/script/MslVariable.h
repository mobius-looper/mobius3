/**
 * Wraps the MslVariableNode for use in Linkages
 */

#pragma once

class MslVariableExport
{
  public:

    MslVariableExport();
    ~MslVariableExport();

    juce::String getName() {
        return (node != nullptr) ? node->name : "";
    }

    // the parse tree for the function definition
    std::unique_ptr<class MslVariable> node;

};


    
