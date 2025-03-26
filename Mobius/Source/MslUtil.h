/**
 * A few utility methods for dealing with data transformation
 * between MSL and the container model.  Needed by both the shell
 * (Supervisor) and kernel (MobiusKernel) so there can't be any assumptions
 * about context.
 *
 * This is similar to ScriptUtil, look at merging them if possible.
 */

#pragma once

class MslUtil
{
  public:

    /**
     * An object that can provide access to structure names.
     */
    class Provider {
      public:
        virtual ~Provider() {}
        // sigh, probably should not be using juce::String in the kernel
        virtual juce::String getStructureName(class Symbol* s, int v) = 0;
    };

    static void mutateActionArgument(class Symbol* s, class MslValue* v, class UIAction* a);

    static void mutateReturn(Provider* p, class Symbol* s, int value, class MslValue* retval);
    
};

