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
 * The ScopeCache provides a mechanism to quickly map group names to group ordinals
 * and is intended for use in the core.  Because core code is not allowed to allocate
 * memory, it avoids juce::String and other high level containers that have the potential
 * for allocation.  It is only used by Actionator and could live down there, but I'm
 * keeping it here so the mapping logic can all be in one place.
 *
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

    /**
     * Parse a scope string as a group name and return the ordinal of the
     * matching group.  Note that this is a zero based ordinal, NOT the group
     * scope number found in lower level code which is 1 based.
     *
     * A MobusConfig must be supplied which is where the GroupDefinitions live.
     */
    static int parseGroupOrdinal(class MobiusConfig* config, const char* scope);
    
};

/**
 * Cache of group names that may be embedded in a core object and used
 * for group name to ordinal mapping.
 */
class ScopeCache
{
  public:

    ScopeCache();
    ~ScopeCache();

    /**
     * Refresh the cache after the configuration changes.
     */
    void refresh(class MobiusConfig* config);

    /**
     * Parse a scope containing a group name into a group ordinal
     * using the name cache. Returns -1 if this is not a group name.
     */
    int parseGroupOrdinal(const char* scope);

    /**
     * Parse a scope containing a group name into a group NUMBER.
     * This is the convention used by core code where groups are
     * numbered from 1 and zero implies "no scope".
     * It is simply 1+ the ordinal.
     */
    int parseGroupNumber(const char* scope) {
        return parseGroupOrdinal(scope) + 1;
    }
    
    /**
     * Parse a scope as a track number.
     * Same as Scope::parseTrackNumber but included here so ScopeCache
     * users can get both in one place.
     */
    int parseTrackNumber(const char* scope) {
        return Scope::parseTrackNumber(scope);
    }
    
  private:

    //
    // Going very old-school on this with a 2D array
    //

    static const int MaxGroupName = 32;
    static const int MaxGroupNames = 32;

    char GroupNames[MaxGroupNames][MaxGroupName];
    int GroupNameCount = 0;

};

    
