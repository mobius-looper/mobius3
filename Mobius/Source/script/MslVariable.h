/**
 * Wraps the MslVariableNode for use in Linkages
 */

#pragma once

class MslVariableExport
{
  public:

    MslVariableExport();
    ~MslVariableExport();

    // the parse tree for the function definition
    std::unique_ptr<class MslVariable> node;

};


    
