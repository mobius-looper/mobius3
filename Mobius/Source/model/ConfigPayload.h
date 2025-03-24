/**
 * Structure to wrap verious configuration objects that need to be
 * sent from the UI into the Kernel.
 *
 * If the object pointer is non-null it should be assumed to have been changed
 * and needs to be integrated.  If the object pointer is null the Kernel must keep
 * the existing version of that object.  One of these will be sent whenever any of them change.
 *
 * Ownership of the objects is expected to be taken and the ConfigPayload wrapper
 * object discarded.
 */

#pragma once

class ConfigPayload
{
  public:

    class Session* session = nullptr;
    class ParameterSets* parameters = nullptr;
    class GroupDefinitions* groups = nullptr;
    
    // this may be where FunctionProperties need to live too

    // todo: now that we have this, it could also be the mechanism for passing
    // down ScriptConfig and SampleConfigs rather than having
    // specific methods for those in MobiusInterface
    // also Binderator

};
