
#pragma once

#include <JuceHeader.h>

class MslContext
{
  public:

    MslContext() {}
    virtual ~MslContext() {}

    virtual juce::File mslGetRoot() = 0;
    virtual class MobiusConfig* mslGetMobiusConfig() = 0;
    virtual void mslDoAction(class UIAction* a) = 0;
    virtual bool mslDoQuery(class Query* q) = 0;

};

