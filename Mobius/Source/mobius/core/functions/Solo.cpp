/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Solo the current track.
 *
 * TODO: Solo doesn't obey quantization or MuteCancelFunctions.
 * Might want this to behave ore like the other mute functions so you
 * can use it as a performance effect without having to write scripts.
 * 
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "../Action.h"
#include "../Event.h"
#include "../Function.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../Mobius.h"
#include "../Mode.h"
#include "../Track.h"

// for group awareness
#include "../../track/LogicalTrack.h"

//////////////////////////////////////////////////////////////////////
//
// SoloMode
//
//////////////////////////////////////////////////////////////////////

/**
 * This is a minor mode we're in when Solo is active.
 * This is a funny one and needs work.  We're actually only showing
 * it on the track that is being soloed, this should be displayed
 * in a more global way so you can tell why other tracks are currently
 * muted.
 */
class SoloModeType : public MobiusMode {
  public:
	SoloModeType();
};

SoloModeType::SoloModeType() :
    MobiusMode("solo")
{
}

SoloModeType SoloModeObj;
MobiusMode* SoloMode = &SoloModeObj;

/****************************************************************************
 *                                                                          *
 *   								 SOLO                                   *
 *                                                                          *
 ****************************************************************************/
/*
 * This is a bit different than Mute because it is a "track level" function
 * that is not sensitive to quantization in the current loop.  Solo happens
 * immediately by unmuting the selected track and muting all others.
 * 
 * This isn't exactly like a console solo button in that if you punch 
 * solo again it doesn't return to the previous state yet.
 */

class SoloFunction : public Function {
  public:
	SoloFunction();
    void invoke(Action* action, Mobius* m);
};

SoloFunction SoloObj;
Function* Solo = &SoloObj;

SoloFunction::SoloFunction() :
    Function("Solo")
{
	global = true;
}

/**
 * Solo global function handler.
 *
 * This is similar to GlobalMute in that we mute everything
 * that is currently playing and restore just those tracks that
 * were playing when solo turns off.  Unlike global mute, one
 * track is designated as the solo track and gets to keep playing.
 * The solo track may have been muted, in which case it will
 * return to being muted when solo is canceled.  Like GlobalMute
 * any other form of mute change directly to a track, cancels
 * solo and global mute, but it leaves the solo track playing.
 *
 * This is declared as a global function but it is actually 
 * sensitive to the selected track and may have a track-specific 
 * binding.  Because it is global it will not have gone through
 * the usual target resolution process in Mobius::resolveTrack
 * so we have to check the track scope here.  It would be easy
 * to make this non-global but then it would be replicated if
 * you selected group scope and the last track in the group would win.
 * By forcing it global we can make decisions about what solo
 * means for the group.  Actually group scope is ignored right now.
 * 
 */
void SoloFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {

        // !! revisit how functions are used to pass down the
        // semantics of calling cancelGlobalMute!
        Function* func = (action != nullptr) ? action->getFunction() : Solo;

        // figure out what state we're in
        // warnings: these were defined but never used
        //bool soloing = false;
        //bool canceling = false;
        //bool globalMute = false;

        int tracks = m->getTrackCount();

        // expecting this to be null but just in case
        // we have another way of forcing it, pay attention
        Track* track = action->getResolvedTrack();
        if (track == nullptr) {
            // arguments trump binding scope?
            if (action->arg.getType() == EX_INT)
              track = m->getTrack(action->arg.getInt() - 1);

            if (track == nullptr) {
                int tnum = action->getTargetTrack();
                if (tnum > 0)
                  track = m->getTrack(tnum - 1);
                else {
                    int group = action->getTargetGroup();
                    if (group > 0) {
                        // we should try to support group solo
                        // for now pick the first one in the group
                        for (int i = 0 ; i < tracks ; i++) {
                            Track* t = m->getTrack(i);
                            LogicalTrack* lt = t->getLogicalTrack();
                            if (lt->getGroup() == group) {
                                track = t;
                                break;
                            }
                        }
                    }
                    else {
                        // default to selected track
                        track = m->getTrack();
                    }
                }
            }
        }

        if (track == nullptr) {
            // must have been an empty group
            Trace(2, "Unable to resolve track to solo\n");
        }
        else if (track->isSolo()) {
            // canceling solo
            for (int i = 0 ; i < tracks ; i++) {
                Track* t = m->getTrack(i);
                if (t->isGlobalMute()) {
                    t->setMuteKludge(func, false);
                    t->setGlobalMute(false);
                }
                else {
                    // should only be unmuted if this
                    // is the solo track
                    t->setMuteKludge(func, true);
                }
                t->setSolo(false);
            }
        }
        else {
            // solo target track
            for (int i = 0 ; i < tracks ; i++) {
                Track* t = m->getTrack(i);
                Loop* l = t->getLoop();

                if (t == track) {
                    // The global mute flag is used in a confusing way here to 
                    // avoid having yet another track flag.  If the solo track
                    // is currently playing set global mute to indicate that the
                    // track needs to stay playing when solo turns off.
                    // This is at least consistent with GlobalMute.

                    if (l->isMuteMode())
                      t->setMuteKludge(func, false);
                    else
                      t->setGlobalMute(true);

                    t->setSolo(true);
                }
                else if (!l->isReset() && !l->isMuteMode()) {
                    t->setGlobalMute(true);
                    t->setMuteKludge(func, true);
                }
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
