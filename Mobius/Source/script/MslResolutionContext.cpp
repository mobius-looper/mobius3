
#include <JuceHeader.h>

#include "MslLinkage.h"
#include "MslFunction.h"
#include "MslVariableExport.h"

#include "MslResolutionContext.h"

MslResolutionContext::MslResolutionContext(class MslGarbage* g)
{
    garbage = g;
}

MslResolutionContext::~MslResolutionContext()
{
    // Linkage will also release whatever it points to
    linkages.clear();
}

class MslLinkage* MslResolutionContext::find(juce::String name)
{
    MslLinkage* link = table[name];
    return link;
}


class MslLinkage* MslResolutionContext::intern(juce::String name, class MslFunction* f)
{
    MslLinkage* link = intern(name);

    if (link->function != nullptr) {
        garbage->add(link->function);
        link->function = f;
    }

    if (link->variable != nullptr) {
        Trace(2, "Warning: Changing linkage %s from variable to function",
              name.toUTF8());
        garbage->add(link->variable);
        link->variable = nullptr;
    }
}

class MslLinkage* intern(juce::String name, class MslVariableExport* v)
{
    MslLinkage* link = intern(name);

    if (link->variable != nullptr) {
        garbage->add(link->variable);
        link->variable = v;
    }

    if (link->function != nullptr) {
        Trace(2, "Warning: Changing linkage %s from function to variable",
              name.toUTF8());
        garbage->add(link->function);
        link->function = nullptr;
    }
}

class MslLinkage* MslResolutionContext::intern(juce::String name)
{
    MslLinkage* link = table[name];
    if (link == nullptr) {
        link = new MslLinkage();
        link->name = name;
        linkages.add(link);
        table.set(name, link);
    }
    return link;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
