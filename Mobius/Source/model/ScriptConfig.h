/*
 * Model for configuration scripts to load
 *
 * This is becomming obsolete after the introduction of ScriptRegistry.
 * It is still used as the transfer model from the UI into Mobius since the
 * old .mos compiler lives down there.  ScriptRefs will be generated at runtime
 * to have the sanitized file paths, and is allowed to maintain a set of
 * error messages.
 *
 * The error message object is very similar to MslError but I don't want to drag
 * in a dependency on that in old code.
 *
 * To make this behave more like MSL, the ScriptRef could be like MslScriptUnit and
 * pass back other information found during parsing, like sustainable flags, etc.  Right now
 * those are assumed to have been left on the Symbols that were created as a side effect of
 * installing.
 */

#pragma once

#include <JuceHeader.h>

// model errors the new way
#include "../script/MslError.h"

/**
 * A simple container of ScriptRefs with some maintenance utilities.
 */
class ScriptConfig {

  public:

    ScriptConfig();
    ~ScriptConfig();
    ScriptConfig* clone();

    class ScriptRef* getScripts();
	void setScripts(class ScriptRef* refs);

	void add(class ScriptRef* ref);
	void add(const char* file);
    class ScriptRef* get(const char* file);
    bool isDifference(ScriptConfig* other);

  private:

	void clear();

    class ScriptRef* mScripts;
};

/**
 * Represents a reference to a Script stored in a file.
 * A list of these is maintained in the ScriptConfig inside MobiusConfig
 * 
 * As of 1.31 mName may either be a file name or a directory name.
 * These are compiled into a ScriptSet with loaded Script objects.
 */
class ScriptRef {

  public:

    ScriptRef();
    ScriptRef(const char* file);
    ScriptRef(ScriptRef* src);
	~ScriptRef();

    void setNext(ScriptRef* def);
    ScriptRef* getNext();

    void setFile(const char* file);
    const char* getFile();

    void setTest(bool b);
    bool isTest();

    // errors encountered during compilation
    juce::OwnedArray<MslError> errors;
    
  private:

    void init();

    ScriptRef* mNext;
    char* mFile;

    // early hack for test scripts, try to get rid of this
    bool mTest;

};

