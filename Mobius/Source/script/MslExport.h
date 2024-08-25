/**
 * Model for a function or variable exported from the MSL environment
 * and accessible to the containing application.
 */

#include <JuceHeader.h>

class MslExport
{
  public:

    MslExport() {}
    ~MslExport() {}

    juce::String name;

    // todo: Might want a signature here, but this
    // isn't yet necessary for the container
    // the internal MslLinkage WILL have a signature for compilation

    // opaque pointer to the MSL internal object that implements this
    // the MslLinkage
    void* linkage = nullptr;

};

