/**
 * Parse tree for the MCL language
 *
 * Most of what is in an MCL file are variable assignemnts of this form:
 *
 *    [scope:] name value
 *
 * "name" is the name of a Mobius parameter and "value" is a number, string, or enumeration
 * keyword
 *
 * Scope is optional and defaults to the current "running scope".   Scopes are numbers
 * followed by the colon and represent logical track numbers.  Eventully should support
 * track names here as well, and maybe provide some track alias declarations at the top.
 *
 * Assignments are performed within an Object Scope.  The default object scope is the
 * active session.
 *
 * Object scope is defined with either the keywords "session" and "overlay".
 *
 * The format of an object scope declartion is:
 *
 *     scope-keyword object-name [lifespan]
 *
 * scope-keyword ::= session | overlay
 * object-name ::= string
 * lifespan ::= temporary | stable | permanent
 *
 * Section Headers:
 *
 * session
 * session active
 *   - modifies the active session
 *   - active is the default if not specified
 *   - the changes are permanent
 *   - the modified session is loaded
 * 
 * session foo
 *   - modifies the session named "foo"
 *   - if the session is also active the session is loaded
 * 
 * session memory
 *   - modifies the active session in memory but does not save it
 *
 * session temporary
 *   - confusing name, but this means to apply the values as if they were performed
 *     by actions so the symbols have temporary bindings that are lost on reset
 *
 * overlay foo
 *   - modifies the overlay with a name
 *
 * overlay foo memory
 *   - when modifying overlays in memory you must specify a name since
 *     there is no single active overlay
 *
 * overlay foo temporary
 *   - effectively the same as a bunch of active track action bindings
 *
 * Assignments:
 
 * syncMode master
 *   - sets the syncMode parameter to SyncMaster in the current running scope
 *   - if the running scope is session, this sets the session defaults
 *
 * 2:syncMode master
 *   - sets the syncMode in track 2 (only allowed in session object scope)
 *   - ignores the running scope
 *
 * *:syncMode=master
 *   - sets a track override in all tracks, similar to setting the
 *     session default but each track can have an independent value
 *
 * scope 1
 *   - sets the running scope to the track number 1
 * scope foo
 *   - sets running scope to track named "foo"
 * scope global
 *   - sets running scope to the session defaults
 *   - this is implicit for "overlay" object scope
 */

#pragma once

#include <JuceHeader.h>

#include "../script/MslValue.h"

/**
 * A single parameter assignment.
 */
class MclAssignment
{
  public:

    // parsing results
    juce::String scopeId;
    juce::String name;
    juce::String svalue;

    // linking results
    class Symbol* symbol = nullptr;
    MslValue value;
    int scope = 0;
    bool remove = false;
    
};

/**
 * Runnimg scope within an object.
 */
class MclScope
{
  public:

    constexpr static const char* Keyword = "scope";
    // ugh, scope is too weird for most people, just use this
    constexpr static const char* AltKeyword = "track";
    
    juce::String scopeId;

    // track number or 0 for global
    int scope = 0;

    juce::OwnedArray<MclAssignment> assignments;

    void add(MclAssignment* a) {
        assignments.add(a);
    }
    
};

/**
 * Object scope
 */
class MclSection
{
  public:

    constexpr static const char* KeywordSession = "session";
    constexpr static const char* KeywordOverlay = "overlay";
    constexpr static const char* KeywordBinding = "binding";
    constexpr static const char* KeywordBindings = "bindings";

    // reserved names
    constexpr static const char* NameActive = "active";
    
    // evaluation options
    constexpr static const char* EvalPermanent = "permanent";
    constexpr static const char* EvalMemory = "memory";
    constexpr static const char* EvalTemporary = "temporary";
    
    typedef enum {
        Session,
        Overlay,
        Binding
    } Type;

    typedef enum {
        Permanent,
        Memory,
        Temporary
    } Duration;
    
    juce::String name;
    Type type = Session;
    Duration duration = Permanent;
    bool replace = false;

    // content for Sessions and overlays
    juce::OwnedArray<MclScope> scopes;

    // content for BindingSets
    juce::OwnedArray<class Binding> bindings;
    bool bindingOverlay = false;
    bool bindingNoOverlay = false;

    // update statistics
    int additions = 0;
    int modifications = 0;
    int removals = 0;
    int ignores = 0;
    
    void add(MclScope* s) {
        scopes.add(s);
    }
    
};

/**
 * One runnable MCL compilation unit.
 * Execution of a script automatically commits changes at the end.
 */
class MclScript
{
  public:
    
    juce::OwnedArray<MclSection> sections;

    void add(MclSection* s) {
        sections.add(s);
    }

};
    
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
