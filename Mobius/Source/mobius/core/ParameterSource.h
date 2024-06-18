/**
 * Temporary intermediary that provides parameter values to the core code.
 * Used to ease the transition away from Preset and Setup and toward
 * ParameterSets and Symbols.
 */

#pragma once

#include "../../model/ParameterConstants.h"

class ParameterSource
{
  public:

    static ParameterMuteMode getMuteMode(class Loop* l, class Event* e);
};

