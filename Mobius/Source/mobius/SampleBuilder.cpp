/**
 * Code related to the construction of a SampleManager and related classes.
 * Code related to the runtime execution are in SampleManger.cpp
 *
 * Since the execution contexts are so different, I like keeping the
 * files separate to reinforce the difference.
 * 
 * All of the construction code runs in the UI thread and does not
 * need any of the runtime code.
 *
 * All of the runtime code must not touch anything in the construction code.
 *
 * A SampleManager is built by MobiusShell when it is sent a ScriptConfig
 * object from the UI, or one is loaded during the inital startup process.
 *
 * The way these are built is different under MobiusShell to avoid file
 * management from within the engine.  The UI is expected to take the
 * SampleConfig out of the MobiusConfig (or wherever it is stored) and
 * cause all of the sample files to be loaded and left as float* arrays
 * in the Sample objects.  This "loaded" config is then given to MobiusShell
 * and converted to a SampleManager.
 */

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../model/SampleConfig.h"

#include "MobiusInterface.h"

#include "Audio.h"
#include "AudioPool.h"

#include "SampleManager.h"
#include "core/Mem.h"

//////////////////////////////////////////////////////////////////////
//
// SampleManager
//
//////////////////////////////////////////////////////////////////////

/**
 * The AudioPool is necessary for the conversion of the raw float*
 * arrays into Audio objects within SamplePlayer.
 * This pool is so fundamental consider having it static?
 *
 * When this finishes the passed SampleConfig will still exist and
 * still contain a list of Sample objects, but the internal float* arrays
 * will have been taken.
 */
SampleManager::SampleManager(AudioPool* pool, SampleConfig* samples) 
{
	mPlayerList = nullptr;
	mSampleCount = 0;
	mLastSample = -1;

    // the player list is represented both as a linked list and as an array
    // the list is authoritative, and the array is build as a cache
    // for fast indexing
	for (int i = 0 ; i < MAX_SAMPLES ; i++)
	  mPlayers[i] = nullptr;

    // build the list of SamplePlayers
    SamplePlayer* last = nullptr;
    if (samples != nullptr) {
        for (Sample* s = samples->getSamples() ; s != nullptr ; s = s->getNext()) {
            SamplePlayer* p = NEW2(SamplePlayer, pool, s);
            if (last == nullptr)
              mPlayerList = p;
            else
              last->setNext(p);
            last = p;
        }
    }

    // now build the index
    SamplePlayer* player = mPlayerList;
    while (player != nullptr) {
        mPlayers[mSampleCount] = player;
        player = player->getNext();
        mSampleCount++;
        if (mSampleCount >= MAX_SAMPLES)
          break;
    }

    // should have consumed them all
    // could have caught this earlier, just leave them
    // there but you won't be able to trigger them
    if (player != nullptr) {
        Trace(1, "SampleManager: Too many samples!\n");
    }
}

/**
 * Delete only the list, the index just references things on the list.
 * Destruction of the SamplePlayer will return the Audio to the pool
 * it came from.
 */
SampleManager::~SampleManager()
{
	delete mPlayerList;
}

/**
 * Compare the sample definitions in a Samples object with the
 * active loaded samples.  If there are any differences it is a signal
 * the caller to reload the samples and phase them in to the next interrupt.
 *
 * Differencing is relatively crude, any order or length difference is
 * considered to be enough to reload the samples.  In theory if we have lots
 * of samples and people make small add/remove actions we could optimize
 * the rereading but for now that's not very important.
 *
 * jsl - this is kept around for reference but not currently used
 * If difference detection is done by MobiusShell, then it is relatively
 * safe for it to obtain to a handle to the live SampleManager for comparison
 * but it's an ugly violation of encapsulation.
 *
 * If the differenceing is done by the kernel, then we have to pass the whole
 * thing down, and if we decide it isn't worth it, pass it back up so it can be
 * discarded.  Not much savings since we loaded everything for no reason.
 *
 * A better approach would to be to do this in the UI before loading to just
 * comapre the before and after SampleConfigs after editing.
 *
 * This is broken anyway because it will crash if the player list is shorter
 * than the new Sample list.
 *
 */
bool SampleManager::isDifference(SampleConfig* samples)
{
    bool difference = false;
    
    if (samples == nullptr) {
        // diff if we have anything
        if (mPlayerList != nullptr)
          difference = true;
    }
    else {
        int srcCount = 0;
        for (Sample* s = samples->getSamples() ; s != nullptr ; s = s->getNext())
          srcCount++;

        int curCount = 0;
        for (SamplePlayer* p = mPlayerList ; p != nullptr ; p = p->getNext())
          curCount++;

        if (srcCount != curCount) {
            // doesn't matter what changed
            difference = true;
        }
        else {
            Sample* s = samples->getSamples();
            SamplePlayer* p = mPlayerList;
            while (s != nullptr && !difference) {
                // note that we're comparing against the relative path
                // not the absolute path we built in the SamplePlayer
                // constructor 
                if (!StringEqual(s->getFilename(), p->getFilename())) {
                    difference = true;
                }
                else {
                    s = s->getNext();
                    p = p->getNext();
                }
            }
        }
    }

    return difference;
}

//////////////////////////////////////////////////////////////////////
//
// SamplePlayer
//
//////////////////////////////////////////////////////////////////////

SamplePlayer::SamplePlayer(AudioPool* pool, Sample* src)
{
	init();
	
    // necessary only for isDifference, could remove when that goes
    mFilename = MemCopyString("SamplePlayer::mFileName", src->getFilename());
	mSustain = src->isSustain();
	mLoop = src->isLoop();
	mConcurrent = src->isConcurrent();
    mButton = src->isButton();
    
    // this is the interesting part
    // create an Audio and fill it with the Sample data
    // I don't believe this "steals" the data, it copies
    // it into a set of segmented AudioBuffers

    mAudio = pool->newAudio();

    AudioBuffer b;
    b.buffer = src->getData();
    b.frames = src->getFrames();
    b.channels = 2;

    // I think we used to capture the sample rate here too
    
    mAudio->append(&b);
}

void SamplePlayer::init()
{
	mNext = nullptr;
    mFilename = nullptr;
	mAudio = nullptr;
	mSustain = false;
	mLoop = false;
	mConcurrent = false;
    mButton = false;
    
    mCursors = nullptr;
    mCursorPool = nullptr;
    mTriggerHead = 0;
    mTriggerTail = 0;
	mDown = false;

	mFadeFrames = 0;
    mInputLatency = 0;
    mOutputLatency = 0;
}

SamplePlayer::~SamplePlayer()
{
    delete mFilename;
	delete mAudio;

    // if we had a global cursor pool, this should
    // return it to the pool instead of deleting
    delete mCursors;
    delete mCursorPool;

    SamplePlayer* nextp = nullptr;
    for (SamplePlayer* sp = mNext ; sp != nullptr ; sp = nextp) {
        nextp = sp->getNext();
		sp->setNext(nullptr);
        delete sp;
    }
}

//////////////////////////////////////////////////////////////////////
//
// SampleCursor
//
//////////////////////////////////////////////////////////////////////

/*
 * Each cursor represents the playback of one trigger of the
 * sample.  To implement the insertion of the sample into
 * the recorded audio stream, we'll actually maintain two cursors.
 * The outer cursor handles the realtime playback of the sample, 
 * the inner cursor handles the "recording" of the sample into the
 * input stream.  
 *
 * Implementing this as cursor pairs was easy since they have to 
 * do almost identical processing, and opens up some interesting
 * possibilities.
 */

/**
 * Constructor for record cursors.
 */
SampleCursor::SampleCursor()
{
    init();
}

/**
 * Constructor for play cursors.
 * UPDATE: I haven't been back here for a long time, but it looks like
 * we're always creating combo play/record cursors.
 */
SampleCursor::SampleCursor(SamplePlayer* s)
{
    init();
	mRecord = NEW(SampleCursor);
	setSample(s);
}

/**
 * Initialize a freshly allocated cursor.
 */
void SampleCursor::init()
{
    mNext = nullptr;
    mRecord = nullptr;
    mSample = nullptr;
	mAudioCursor = NEW(AudioCursor);
    mStop = false;
    mStopped = false;
    mFrame = 0;
    mMaxFrames = 0;
}

SampleCursor::~SampleCursor()
{
	delete mAudioCursor;
    delete mRecord;

    SampleCursor *el, *next;
    for (el = mNext ; el != nullptr ; el = next) {
        next = el->getNext();
		el->setNext(nullptr);
        delete el;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
