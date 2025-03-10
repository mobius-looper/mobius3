/*
 * An XML generator for configuration objects.
 *
 * MobiusConfig
 *   contains individual global parameters and lists of sub-objects
 *
 * Preset
 *   a collection of operational parameters for functions
 *
 * Setup
 *   a collection of operational parameters for tracks
 *
 * BindingSet
 *   a collection of bindings between external triggers and internal targets
 */

#pragma once

#include "../SymbolId.h"
#include "../ParameterConstants.h"

class XmlRenderer {

  public:

    XmlRenderer(class SymbolTable* st);
    ~XmlRenderer();

    class MobiusConfig* parseMobiusConfig(const char* xml);
    char* render(class MobiusConfig* c);

    class MobiusConfig* clone(class MobiusConfig* src);
    class Preset* clone(class Preset* src);
    class Setup* clone(class Setup* src);

  private:

    // common utilities

    //void render(class XmlBuffer* b, class UIParameter* p, int value);
    void render(class XmlBuffer* b, SymbolId sid, int value);
    //void render(class XmlBuffer* b, class UIParameter* p, bool value);
    void render(class XmlBuffer* b, SymbolId sid, bool value);
    //void render(class XmlBuffer* b, class UIParameter* p, const char* value);
    void render(class XmlBuffer* b, SymbolId sid, const char* value);
    void render(class XmlBuffer* b, const char* name, const char* value);
    void render(XmlBuffer* b, const char* name, int value);

    //int parse(class XmlElement* e, class UIParameter* p);
    int parse(class XmlElement* e, SymbolId sid);
    int parse(XmlElement* e, const char* name);
    
    //const char* parseString(class XmlElement* e, class UIParameter* p);
    const char* parseString(class XmlElement* e, SymbolId sid);
    
    class StringList* parseStringList(class XmlElement* e);
    void renderList(class XmlBuffer* b, const char* elname, class StringList* list);

    void renderStructure(class XmlBuffer* b, class Structure* s);
    void parseStructure(class XmlElement* e, class Structure* s);

    // main objects

    void render(class XmlBuffer* b, class MobiusConfig* c);
    void parse(class XmlElement* e, class MobiusConfig* c);

    void render(class XmlBuffer* b, class Preset* p);
    void parse(class XmlElement* e, class Preset* p);

    void render(class XmlBuffer* b, class Setup* s);
    void parse(class XmlElement* b, class Setup* s);

    void render(class XmlBuffer* b, class SetupTrack* t);
    void parse(class XmlElement* b, class SetupTrack* t);

    void render(class XmlBuffer* b, class UserVariables* container);
    void parse(class XmlElement* e, class UserVariables* container);

    void parse(class XmlElement* e, class BindingSet* c);
    void render(class XmlBuffer* b, class BindingSet* c);

    void parse(class XmlElement* e, class Binding* c);
    void render(class XmlBuffer* b, class Binding* c);

    void render(class XmlBuffer* b, class ScriptConfig* c);
    void parse(class XmlElement* b, class ScriptConfig* c);

    void render(class XmlBuffer* b, class SampleConfig* c);
    void parse(class XmlElement* b, class SampleConfig* c);

    void render(class XmlBuffer* b, class OscConfig* c);
    void parse(class XmlElement* b, class OscConfig* c);

    void render(class XmlBuffer* b, class OscBindingSet* obs);
    void parse(class XmlElement* e, class OscBindingSet* obs);

    void render(class XmlBuffer* b, class OscWatcher* w);
    void parse(class XmlElement* e, class OscWatcher* w);
    
    void render(class XmlBuffer* b, class GroupDefinition* g);
    void parse(class XmlElement* e, class GroupDefinition* g);

    class SymbolTable* symbols = nullptr;
    const char* getSymbolName(SymbolId sid);


    const char* render(OldSyncSource src);
    OldSyncSource parseOldSyncSource(const char* value);
    const char* render(OldSyncUnit unit);
    const char* render(SyncTrackUnit unit);
    SyncTrackUnit parseSyncTrackUnit(const char* value);

};
