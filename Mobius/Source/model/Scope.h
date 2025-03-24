/**
 * Utilities to deal with scope strings.
 *
 * Scopes originate in Bindings, set by the BindingEditor.  They are copied into
 * UIAction and PluginParameter on their way from the UI into the core.  Once in
 * the core, scopes are converted into integers represengting either a track number
 * or a group number as used by the old Action model.  It is not necessary for code
 * above core to understand what scope strings mean.
 * 
 *  The meaning of the string is one of the following:
 *
 *     empty string or "0"
 *        indicates no scope, meaning the active track
 * 
 *     track number
 *        a 2 digit 1 based track number
 *
 *     group name
 *        the user defined name of a track group
 *
 * In old Mobius, group scope was indiciated with a single letter starting from 'A'.
 * New code should make no assumptions about that.  Names may be single letters or
 * anything else the user types in the GroupEditor.
 *
 * This used to have a group name cache and provide tools for mapping group names
 * to ordinals but GroupDefinitions does that now.
 */

#pragma once

/**
 * Static methods for scope parsing.
 */
class Scope
{
  public:

    /**
     * Parse a scope string as a track number.
     * Returns 0 if empty or a positive integer if this is a two digit number.
     * Returns -1 if this is not a number, meaning it must be a group name.
     */
    static int parseTrackNumber(const char* scope);

};
