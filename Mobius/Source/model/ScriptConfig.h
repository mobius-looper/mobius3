/*
 * Model for configuration scripts to load
 */

#pragma once

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
    
  private:

    void init();

    ScriptRef* mNext;
    char* mFile;

    // early hack for test scripts, try to get rid of this
    bool mTest;

};

