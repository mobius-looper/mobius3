
#include <JuceHeader.h>

#include "../../model/UIAction.h"
#include "../../model/Query.h"

#include "../MobiusInterface.h"
#include "../MobiusKernel.h"

#include "MidiTracker.h"

MidiTracker::MidiTracker(MobiusContainer* c, MobiusKernel* k)
{
    container = c;
    kernel = k;
}

MidiTracker::~MidiTracker()
{
}

void MidiTracker::initialize()
{
}

void MidiTracker::reconfigure()
{
}

void MidiTracker::doAction(UIAction* a)
{
    (void)a;
}

bool MidiTracker::doQuery(Query* q)
{
    (void)q;
    return true;
}



  
