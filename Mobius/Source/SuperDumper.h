/**
 * Extension of StructureDumper that teaches it the ways
 * of Supervisor and it's root, and has a name that needed to be done.
 */

#pragma once

#include "util/StructureDumper.h"

class SuperDumper : public StructureDumper
{
  public:
    SuperDumper(class Supervisor* super);
    virtual ~SuperDumper() {}

    void write(juce::String filename) override;

    void reset() {
        counter = 0;
    }
    
  private:
    class Supervisor* supervisor = nullptr;
    int counter = 0;
    
};
