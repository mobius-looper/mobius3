/**
 * Represents a collision between reference name symbols.  The collision
 * may be resolved by renaming the script or export, or by unloading one
 * of the other scripts.
 *
 * These are created by MalEnvironment as files are loaded.
 * They are shell level objects and do not have to use the pool.
 */

#pragma once

class MslCollision
{
  public:
    MslCollision() {}
    ~MslCollision() {}

    // the name that is in conflict
    juce::String name;

    // the script that wanted to install the duplicadte name
    juce::String fromPath;

    // the script that already claimed this name
    juce::String otherPath;

    // todo remember whether this was a collision on the outer script
    // name or an exported proc/var inside it
};

