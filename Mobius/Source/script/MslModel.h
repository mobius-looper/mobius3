/**
 * Parse tree model for MSL scripts
 */

#pragma once

#include <JuceHeader.h>


class MslStatement
{
  public:
    MslStatement() {}
    virtual ~MslStatement() {}

};

class MslBlock : public MslStatement
{
  public:
    MslBlock() {}
    virtual ~MslBlock() {}

    juce::<OwnedArray> statements;

};

class MslScript : public MslBlock
{
  public:
    MslScript() {}
    ~MslScript() {}

    juce::String name;
    
};

class MslVar : public MslStatement
{
  public:
    MslVar() {}
    ~MslVar() {}

    juce::String name;
    
};

class MslProc : public MslBlock
{
  public:
    MslProc() {}
    ~MslProc() {}

    juce::String name;
    
};

class MslFunction : public MslStatement
{
  public:
    MslFunction() {}
    ~MslFunction() {}

    class Symbol* symbol = nullptr;
};

class MslAssignment : public MslStatement
{
  public:
    MslFunction() {}
    ~MslFunction() {}

    class Symbol* symbol = nullptr;
};

