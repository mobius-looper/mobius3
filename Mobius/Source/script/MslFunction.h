/**
 * An object representing a callable thing within the MSL environment.
 *
 * Wraps an MslFunctionNode.
 *
 * public fields are accessible by the application
 * protected fields are only accessible within the environment
 *
 */

#pragma once

class MslFunction
{
    friend class MslEnvironment;
    friend class MslLinker;
    
  public:

    MslFunction();
    ~MslFunction();

    // reference name of the function
    juce::String name;

    // various declaration results
    // todo: lots more here
    bool sustainable = false;

  protected:

    // the parse tree for the function definition
    std::unique_ptr<class MslFunctionNode> node;

};


    
