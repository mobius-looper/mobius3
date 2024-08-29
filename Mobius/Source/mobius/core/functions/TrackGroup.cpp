/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Track group assignment.
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "../../../model/MobiusConfig.h"

#include "../Action.h"
#include "../Event.h"
#include "../Function.h"
#include "../Loop.h"
#include "../Mobius.h"
#include "../Track.h"

/****************************************************************************
 *                                                                          *
 *                                TRACK GROUP                               *
 *                                                                          *
 ****************************************************************************/

class TrackGroupFunction : public Function {
  public:
	TrackGroupFunction();
	Event* invoke(Action* action, Loop* l);
	void invokeLong(Action* action, Loop* l);
};

TrackGroupFunction TrackGroupObj;
Function* TrackGroup = &TrackGroupObj;

TrackGroupFunction::TrackGroupFunction() :
    Function("TrackGroup")
{
	longPressable = true;
}

void TrackGroupFunction::invokeLong(Action* action, Loop* l)
{
    (void)action;
    Track* t = l->getTrack();

	t->setGroup(0);
}

/**
 * This underwent some changes in Mobius 3 to support group names.
 * No longer accepting letters unless it is also the name.
 * Numbers must be from 1
 *
 * Binding arguments and UIAction/Action conversion is weird...
 * When you type in a number in the binding args text area in the UI
 * it gets set as both UIAction.value and UIAction.arguments
 * I think this is for simple functions that only expect numeric arguments.
 *
 * When UIAction is converted to Action the UIAction.value gets set in the "arg"
 * ExValue as an Int, and the UIAction.arguments is copied to Action.bindingArgs.
 *
 * In retrospect, Action.bindingArgs isn't necessary since we've got an ExValue there
 * it could have just done the number sniffing and set the ExValue to an Int or
 * set it to a String and used the same ExValue for both.
 *
 * Here, if the entered a keyword, the ExValue won't have an EX_STRING, the UIAction.value
 * will have been left zero which becomes the value for the Action.arg.
 * 
 * So if we have Action.bindingArgs THAT is where the group name or keywords has to come from
 * and we may as well use it for the number too.
 * 
 */
Event* TrackGroupFunction::invoke(Action* action, Loop* l)
{
    // new: since we advertise sustainable we'll get up transitions too, ignore them
    if (action->down) {
        Track* t = l->getTrack();
        Mobius* m = l->getMobius();
        MobiusConfig* config = m->getConfiguration();

        // the default is to unset the current group
        int group = 0;
    
        if (strlen(action->bindingArgs) > 0) {

            group = parseBindingArgument(t, config, action->bindingArgs);
        }
        else if (action->arg.getType() == EX_INT) {
            int g = action->arg.getInt();
            if (g >= 0 && g <= config->groups.size()) {
                group = g;
            }
            else if (g < 0) {
                // in the past -1 was a hack to do positive cycling
                // old scripts may still use that
                group = parseBindingArgument(t, config, "next");
            }
            else {
                Trace(1, "TrackGroup: Group number out of range %d", g);
                group = -1;
            }
        }
        else if (action->arg.getType() == EX_STRING) {
            // if bindingArgs didn't come through, and it wasn't treated as an EX_INT
            // treat empty like zero and unset?
            const char* s = action->arg.getString();
            if (strlen(s) > 0)
              group = parseBindingArgument(t, config, s);
        }
    
        if (group >= 0)
          t->setGroup(group);
    }
}

/**
 * Here we have a string group specifier from the binding argument.
 */
int TrackGroupFunction::parseBindingArgument(Track* t, MobiusConfig* config, const char* s)
{
    int group = 0;
    
    if (strlen(s) > 0) {
        
        int gnumber = 1;
        for (auto g : config->groups) {
            // what about case on these?
            if (g->name.equalsIgnoreCase(action->arg.getString())) {
                group = gnumber;
                break;
            }
            gnumber++;
        }
        
        if (group == 0) {
            // name didn't match
            // accept a few cycle keywords
            group = parseCycle(t, config, s);
        }
    }
}

int TrackGroupFunction::parseCycle(const char* token, MobiusConfig* config, Track* t)
{
    int delta = 0;
    
    if ((strcmp(token, "cycle") == 0) || (strcmp(token, "next"))) {
        delta = 1;
    }
    else if (strcmp(token, "prev")) {
        delta = -1;
    }

    int group = t->getGroup() + delta;
    
    if (group > config->groups.size())
      group = 0;
    else if (group < 0)
      group = config->groups.size();

    return group;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
