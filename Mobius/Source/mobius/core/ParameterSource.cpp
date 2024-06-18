/**
 * Temporary intermediary that provides parameter values to the core code.
 * Used to ease the transition away from Preset and Setup and toward
 * ParameterSets and Symbols.
 *
 * Starting access method is just to pull the Preset from the Loop which
 * will also be the same as the Preset in the Loop's Track.
 */

#pragma once

#include "../../model/ParameterConstants.h"
#include "../../model/Preset.h"

#include "Loop.h"
#include "Event.h"
#include "ParameterSource.h"

/**
 * This is one of the few that looked on the Event for the Preset.
 * Remove this since it wasn't used consistently.
 */
ParameterMuteMode ParameterSource::getMuteMode(class Loop* l, class Event* e)
{
    Preset* p = e->getEventPreset();
    if (p == nullptr)
      p = l->getPreset();
    return p->getMuteMode();
}

