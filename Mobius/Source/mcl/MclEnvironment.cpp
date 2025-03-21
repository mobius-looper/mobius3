
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../Provider.h"
// so we can delete MslParser
#include "../model/old/OldBinding.h"

#include "MclModel.h"
#include "MclParser.h"
#include "MclEvaluator.h"
#include "MclEnvironment.h"

MclEnvironment::MclEnvironment(Provider* p)
{
    provider = p;
}

MclEnvironment::~MclEnvironment()
{
}

MclResult MclEnvironment::eval(juce::File f)
{
    juce::String src = f.loadFileAsString();
    return eval(src);
}

MclResult MclEnvironment::eval(juce::String src)
{
    MclResult result;

    MclParser p (provider);
    MclScript* s = p.parse(src, result);
    if (s != nullptr) {
        MclEvaluator e (provider);
        e.eval(s, result);
        delete s;
    }
    return result;
}
