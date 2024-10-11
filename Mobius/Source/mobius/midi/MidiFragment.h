/**
 * Packages a MidiSequence with a location and list links.
 *
 * This started life as a way to do playback "checkpoints" which serve
 * a similar function as segment prefixes, they contain note that are being held
 * at a moment in time within a layer.  These are used to restore held notes without
 * having to scan the layer from the beginning when jumping to a random location
 * as is done when using Undo or LoopSwitch.
 *
 * It could be more general than that, hence the name Fragment.  You could also
 * just add the extra state to MidiSequence to avoid another pooled object class.  Reconsider
 * that if we don't find other uses for this.
 */

#pragma once

#include "../../model/ObjectPool.h"
#include "../../midi/MidiSequence.h"

class MidiFragment : public PooledObject
{
  public:
    MidiFragment();
    ~MidiFragment();

    void poolInit();
    void dump(class StructureDumper& d);
    void clear(class MidiPools* p);
    void copy(class MidiPools* pools, MidiFragment* src);
    
    MidiFragment* next = nullptr;
    MidiFragment* prev = nullptr;

    int frame;
    
    MidiSequence sequence;

  private:

};

class MidiFragmentPool : public ObjectPool
{
  public:

    MidiFragmentPool();
    virtual ~MidiFragmentPool();

    class MidiFragment* newFragment();

  protected:
    
    virtual PooledObject* alloc() override;
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
