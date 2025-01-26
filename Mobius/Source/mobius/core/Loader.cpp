/**
 * New class to encapsulate code related to loop/project loading.
 *
 * I'm trying to evolve the Project concept and consolidate various bits
 * of code strewn through the Track/Loop/Layer/Segment levels that are old
 * and fidgity and I don't want to touch too much old code.
 *
 * Also an exercise in remember how the hell this shit worked.
 */

#include "../Audio.h"

#include "Layer.h"
#include "Loop.h"
#include "Track.h"
#include "Synchronizer.h"
#include "Mobius.h"

#include "Loader.h"

/**
 * Recreate various levels of old code to get an Audio object
 * passed down from the UI into a Loop.
 *
 * trackNumber zero means "active track".
 * loopNumber zero means "next empty loop".
 *
 * todo: unclear whether trying to install over an existing loop
 * should replace, pick the next empty loop, or raise an alert.
 *
 */
void Loader::loadLoop(Audio* audio, int trackNumber, int loopNumber)
{
    bool installed = false;
    
    Track* track = nullptr;
    if (trackNumber ==  0)
      track = mobius->getTrack();
    else
      track = mobius->getTrack(trackNumber - 1);

    if (track != nullptr) {
        Loop* loop = nullptr;

        if (loopNumber > 0) {
            int loopIndex = loopNumber - 1;
            if (loopIndex < track->getLoopCount()) {
                loop = track->getLoop(loopIndex);
            }
            else {
                Trace(1, "Loader: Loop index out of range %d\n", loopIndex);
            }
        }
        else if (loopNumber == 0) {
            // start with the active loop
            Loop* active = track->getLoop();

            // old notes say this doesn't distinguish between "has no frames"
            // and "in a state of reset"
            // not sure if it makes a difference, but do a full reset() just in case
            if (active->isEmpty()) {
                loop = active;
            }
            else {
                // look for an empty one
                // NOTE: the loop "number" is 1 based for reasons
                // Track::getLoop(int) is the "index" which is zero based
                int startIndex = active->getNumber() - 1;
                int loopCount = track->getLoopCount();
                int nextIndex = startIndex + 1;
                // wrap
                if (nextIndex >= loopCount) nextIndex = 0;
                
                while (nextIndex != startIndex && loop == nullptr) {
                    Loop* nextLoop = track->getLoop(nextIndex);
                    if (nextLoop->isEmpty()) {
                        loop = nextLoop;
                    }
                    else {
                        nextIndex++;
                        if (nextIndex >= loopCount) nextIndex = 0;
                    }
                }
            }

            if (loop == nullptr) {
                // todo: either force replace the active loop, or raise an alert
                Trace(1, "Loader: Track is full\n");
            }
        }
        else {
            Trace(1, "Loader: Invalid loop number %d\n", loopNumber);
        }

        if (loop != nullptr) {
            // it said it was empty, make sure it is in full reset
            loop->reset(nullptr);
            Layer* layer = buildLayer(audio);

            // this is a replacement for Loop::loadProjet
            // it has to be inside Loop to access private members

            // the active flag decides whether to immediately put the
            // loop in play mode or put it in a "pause mute"
            // experiment with this
            loop->loadLoopNew(layer, false);
                
            installed = true;

            // todo: Project had options to move the active
            // should we auto select it without LoopCopy behavior?

            // kludge: set this goofy flag to tell the UI that it better
            // pay closer attention to this track and do a full refresh
            //int trackIndex = track->getRawNumber();

            // !! this set a special flag in the old state object to force
            // a refresh, I don't want the new SystemState to require this,
            // state/view refresh should be triggered at a much higher level
            //OldMobiusTrackState* tstate = mobius->getTrackState(trackIndex);
            //tstate->needsRefresh = true;

            // some old comments from Mobius::loadProjectInternal
            
            // Kludge: Synchronizer wants to be notified when
            // we load individual loops, but we're using
            // incremental projects to do that. Rather than
            // calling loadProject() call loadLoop() for
            // each track.
            // !! Revisit this, it would be nice to handle
            // these the same way

            // is this still relevant?
            // if the loop isn't empty, and track sync master isn't set
            // we'll make this track the sync master
            // if outSyncMaster isn't set and this track is configured
            // for SYNC_OUT, make it the outSyncMster
            // not sure about this, just because you dropped a file in a loop
            // doesn't necessarily mean you want this to be the sync master always?
            if (loop == track->getLoop())
              mobius->getSynchronizer()->loadLoop(loop);
            
        }
    }

    if (!installed)
      delete audio;
}

/**
 * Projects were allowed to build Layers which were then passed
 * down into Loop::loadProject for isntallation.
 * Recreate this.
 */
Layer* Loader::buildLayer(Audio* audio)
{
    LayerPool* pool = mobius->getLayerPool();
    Layer* layer = pool->newLayer(nullptr);

    // ProjectLayer could have an id, I guess it can be left zero
    // if it's the only one?
    
    layer->setAudio(audio);

    // outside of a project we don't have enough
    // context to know how many cycles there were
    layer->setCycles(1);

    // all the varioius fade/reverse flags default

    // don't seem to need Segments
    // if I remember right, Segmenets are just referenmces
    // to other Layers which we don't need when loading fresh loops

    return layer;
}
