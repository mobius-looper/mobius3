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
    ScriptConfig(class XmlElement* e);
    ~ScriptConfig();
    ScriptConfig* clone();

    class ScriptRef* getScripts();
	void setScripts(class ScriptRef* refs);

	void add(class ScriptRef* ref);
	void add(const char* file);
    class ScriptRef* get(const char* file);
    bool isDifference(ScriptConfig* other);

    void parseXml(class XmlElement* e);
    void toXml(class XmlBuffer* b);

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
    ScriptRef(class XmlElement* e);
    ScriptRef(ScriptRef* src);
	~ScriptRef();

    void setNext(ScriptRef* def);
    ScriptRef* getNext();

    void setFile(const char* file);
    const char* getFile();

    void parseXml(class XmlElement* e);
    void toXml(class XmlBuffer* b);

    void setTest(bool b);
    bool isTest();
    
  private:

    void init();

    ScriptRef* mNext;
    char* mFile;
    bool mTest;

};

