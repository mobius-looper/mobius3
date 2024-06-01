/**
 * Maintains a pool of audio buffers.
 * There is normally only one of these in a Mobius instance.
 *
 * Broke this out of Audio so we have more control over who uses it.
 */

#pragma once

// for CriticalSection, unfortunate that the users will drag that in
#include <JuceHeader.h>

/**
 * This structure is allocated at the top of every Audio buffer.
 */
struct OldPooledBuffer {

	// todo, pool status, statistics
	OldPooledBuffer* next;
	int pooled;

};

class AudioPool {
    
  public:

    AudioPool();
    ~AudioPool();

    void init(int buffers);
    void dump();

    class Audio* newAudio();
    // class Audio* newAudio(const char* file);
    void freeAudio(class Audio* a);

    float* newBuffer();
    void freeBuffer(float* b);

  private:

    //class CriticalSection* mCsect;
    juce::CriticalSection mCsect;
    
    OldPooledBuffer* mPool;
    //class SampleBufferPool* mNewPool;
    int mAllocated;
	int mInUse;

};

