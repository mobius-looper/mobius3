/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * SaveLoop - a "quick save" of the active loop,.
 *
 */

#include <JuceHeader.h>
#include "../../AudioFile.h"
//#include "../../../SuperDumper.h"

#include <stdio.h>
#include <memory.h>

#include "../Action.h"
#include "../Event.h"
#include "../Function.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../Mobius.h"
#include "../Track.h"

//////////////////////////////////////////////////////////////////////
//
// SaveLoopFunction
//
//////////////////////////////////////////////////////////////////////

class SaveLoopFunction : public Function {
  public:
	SaveLoopFunction();
	void invoke(Action* action, Mobius* m);
  private:
	bool stop;
	bool save;
//    SuperDumper dumper;
};

SaveLoopFunction SaveLoopObj;
Function* SaveLoop = &SaveLoopObj;

SaveLoopFunction::SaveLoopFunction() :
    Function("SaveLoop")
{
	global = true;
	noFocusLock = true;
}

// static Layer* KludgeLayerSave = nullptr;

extern int BlockNumber;

void SaveLoopFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

        m->getTrack()->getLoop()->kludgeSavePlayLayer();
#if 0
        Audio* a = m->getTrack()->getLoop()->getPlaybackAudio();
        juce::File file ("c:/dev/hacklooop.wav");
        AudioFile::write(file, a);
        delete a;
#endif

#if 0        
        dumper.clear();
        m->dump(dumper);
        dumper.write("dump.txt");
#endif
//        Trace(1, "SaveLoop: block number %d\n" , BlockNumber);
//        m->dump("saveLoop.txt", m->getTrack()->getLoop());

		m->saveLoop(action);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
