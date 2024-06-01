/**
 * Extension of StructureDumper that teaches it the ways
 * of Supervisor and it's root, and has a name that needed to be done.
 */

#include "Supervisor.h"
#include "SuperDumper.h"

SuperDumper::SuperDumper()
{
}

void SuperDumper::write(juce::String filename)
{
    setRoot(Supervisor::Instance->getRoot());
    
    if (counter > 0)
      filename += juce::String(counter);

    StructureDumper::write(filename);
}
