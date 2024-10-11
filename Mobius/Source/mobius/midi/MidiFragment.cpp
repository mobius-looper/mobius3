
#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "MidiFragment.h"

MidiFragment::MidiFragment()
{
}

MidiFragment::~MidiFragment()
{
    if (sequence.size() > 0)
      Trace(1, "MidiFragment: Non-empty sequence at destriction");
}

void MidiFragment::poolInit()
{
    next = nullptr;
    prev = nullptr;
    frame = 0;
}

void MidiFragment::dump(StructureDumper& d)
{
    d.start("Fragment:");
    d.add("frame", frame);
    d.newline();

    if (sequence.size() > 0) {
        d.inc();
        sequence.dump(d);
        d.dec();
    }
}

void MidiFragment::clear(MidiEventPool* pool)
{
    sequence.clear(pool);
}

//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

MidiFragmentPool::MidiFragmentPool()
{
    setName("MidiFragment");
    setObjectSize(sizeof(MidiFragment));
    fluff();
}

MidiFragmentPool::~MidiFragmentPool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
PooledObject* MidiFragmentPool::alloc()
{
    return new MidiFragment();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
MidiFragment* MidiFragmentPool::newFragment()
{
    return (MidiFragment*)checkout();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
