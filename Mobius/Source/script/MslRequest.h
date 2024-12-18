/**
 * An object used by the application to ask the environment to do something,
 * either run script code or change the value of a variable.
 *
 * Conceptually similar to UIAction in Mobius which always goes through
 * ActionAdapter to do the model translation.
 *
 * It is different enough from MslAction  that I decided to make it it's own
 * thing rather than have a shared one with unused fields.
 *
 * The call can target either a script or a function exported from a script.
 * Arguments are specified witha list of MslBinding or MslValues that must be allocdated
 * from the pool.
 *
 * MslLinkage is effectively the same as Symbol in a UIAction.
 */

#pragma once

class MslRequest
{
  public:

    MslRequest() {}
    ~MslRequest() {}

    /**
     * The function to call or the variable to set.
     */
    class MslLinkage* linkage = nullptr;

    /**
     * For script calls, a set of named arguments that can be used
     * as an alternative to the "arguments" list which can only be referenced
     * positionally with $x.  Normally only one of bindings or arguments will
     * be set in the request.  In theory should allow both and merge them, in the
     * same way that function call keywowrd arguments are assembled using both named
     * and position arguments.
     *
      * These must be pooled or freely allocadted objects and ownership will
     * be taken by the environment.
     */
    class MslBinding* bindings = nullptr;

    /**
     * For function/script calls, optional positional arguments to the script.
     * For variable assignments, the value to assign.
     *
     * These must be pooled or freely allocadted objects and ownership will
     * be taken by the environment.
     */
    class MslValue* arguments = nullptr;

    /**
     * When non-zero this request came from a sustainable trigger, and the
     * envionment needs to prepare to receive another request later with the same
     * id and the release flag set.  This is relevant only for #sustain scripts.
     *
     * todo: This could also be relevant for #repeat scripts but in practice
     * you won't have the same script bound to two different triggers so we
     * don't need to match them?
     */
    int triggerId = 0;

    /**
     * When non-zero, specifies the default scope this script will logically
     * be running in.  If not set the scope will also not be set in any
     * MslAction or MslQuery calls made back to the application and it must
     * choose an appropriate default.
     */
    int scope = 0;

    /**
     * True if this represents the release of a sustainable trigger.
     */
    bool release = false;

    /**
     * Move the contents of one request to another.
     * This is what happens when a request passed to MslEnvironment has to be
     * sent to the other context through an MslMessage.
     */
    void transfer(MslRequest* src) {
        if (src != nullptr) {
            linkage = src->linkage;
            bindings = src->bindings;
            arguments = src->arguments;
            triggerId = src->triggerId;
            scope = src->scope;
            release = src->release;
            // ownership transfers
            src->bindings = nullptr;
            src->arguments = nullptr;
        }
    }

    // pool initializer when used inside the MslMessage
    void init() {
        linkage = nullptr;
        bindings = nullptr;
        arguments = nullptr;
        triggerId = 0;
        scope = 0;
        release = false;
    }
    
};

