/**
 * Implementation of AudioPool factored out of Audio
 */

// for CriticalSection/ScopedLock
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "Audio.h"
#include "AudioPool.h"

#include "core/Mem.h"

/**
 * Create an initially empty audio pool.
 * There is normally only one of these in a Mobius instance.
 * Should have another list for all buffers outstanding?
 */
AudioPool::AudioPool()
{
    // mCsect = new CriticalSection("AudioPool");
    // juce::CriticalSection is a member object
    mPool = nullptr;
    mAllocated = 0;
    mInUse = 0;

    // needs more testing
    // !! channels
    //mNewPool = new SampleBufferPool(FRAMES_PER_BUFFER * 2);
    //mNewPool = nullptr;
}

/**
 * Release the kracken.
 */
AudioPool::~AudioPool()
{
    Trace(2, "AudioPool: Destructing\n");
    // delete mCsect;
    // delete mNewPool;

    OldPooledBuffer* next = nullptr;
    for (OldPooledBuffer* p = mPool ; p != nullptr ; p = next) {
        next = p->next;
        delete p;
    }
}

/**
 * Allocate a new Audio in this pool.
 * We could pool the outer Audio object too, but the buffers are
 * the most important.
 */
Audio* AudioPool::newAudio()
{
    return NEW1(Audio, this);
}

/**
 * Allocate a new Audio in this pool and read a file.
 */
#if 0
Audio* AudioPool::newAudio(const char* file)
{
    return new Audio(this, file);
}
#endif

/**
 * Return an Audio to the pool.
 * These aren't actually pooled, just free the buffers which
 * will happen in the destructor.
 */
void AudioPool::freeAudio(Audio* a)
{
    a->free();
}

/**
 * Allocate a new buffer, using the pool if available.
 * In theory have to have a different pool for each size, assume only
 * one for now.
 * !! channels
 */
float* AudioPool::newBuffer()
{
	float* buffer;

    /*
      if (mNewPool) {
      buffer = mNewPool->allocSamples();
      }
      else {
    */

    // mCsect->enter();
    { const juce::ScopedLock lock (mCsect);
        
		if (mPool == nullptr) {
			int bytesize = sizeof(OldPooledBuffer) + (BUFFER_SIZE * sizeof(float));
			char* bytes = new char[bytesize];
            MemTrack(bytes, "AudioPool:newBuffer", bytesize);
			OldPooledBuffer* pb = (OldPooledBuffer*)bytes;
			pb->next = nullptr;
			pb->pooled = 0;
			buffer = (float*)(bytes + sizeof(OldPooledBuffer));
            mAllocated++;
		}
		else {
			buffer = (float*)(((char*)mPool) + sizeof(OldPooledBuffer));
			if (!mPool->pooled)
			  Trace(1, "Audio buffer in pool not marked as pooled!\n");
			mPool->pooled = 0;
			mPool = mPool->next;
		}
		mInUse++;
		//mCsect->leave();
    }

    // in both cases, make sure its empty
    // !! these are big, need to keep the list clean and do it
    // in a worker thread
    memset(buffer, 0, BUFFER_SIZE * sizeof(float));

    //	}

	return buffer;
}

/**
 * Return a buffer to the pool.
 */
void AudioPool::freeBuffer(float* buffer)
{
	if (buffer != nullptr) {

		//if (mNewPool != nullptr) {
        //mNewPool->freeSamples(buffer);
        //}
		//else {
        
        OldPooledBuffer* pb = (OldPooledBuffer*)
            (((char*)buffer) - sizeof(OldPooledBuffer));

        if (pb->pooled)
          Trace(1, "Audio buffer already in pool!\n");
        else {
            const juce::ScopedLock lock (mCsect);
            //mCsect->enter();
            pb->next = mPool;
            pb->pooled = 1;
            mPool = pb;
            mInUse--;
            //mCsect->leave();
        }
        // }
	}
}

void AudioPool::dump()
{
	//if (mNewPool != nullptr) {
    // need a dump method for the new one
    //trace("NewBufferPool: %d in use ?? in pool\n", mInUse);
    //}
	//else {
    
    int pooled = 0;
    { const juce::ScopedLock lock (mCsect);
        
        //mCsect->enter();
        for (OldPooledBuffer* p = mPool ; p != nullptr ; p = p->next)
          pooled++;
        //mCsect->leave();
    }

    int used = mAllocated - pooled;

    Trace(2, "AudioPool: %d buffers allocated, %d in the pool, %d in use\n",
           mAllocated, pooled, used);

    // this should match
    if (used != mInUse)
      Trace(2, "AudioPool: Unmatched usage counters %d %d\n",
             used, mInUse);

    //}
}

/**
 * Warm the buffer pool with some number of buffers.
 * Was never implemented.
 */
void AudioPool::init(int buffers)
{
    (void)buffers;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
