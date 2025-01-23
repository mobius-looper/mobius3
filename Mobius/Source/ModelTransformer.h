/**
 * Utility to convert between the old MobiusConfig and Setup models
 * and the new Session model.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "model/Session.h"
#include "model/SymbolId.h"

class ModelTransformer
{
  public:

    ModelTransformer(class Provider* p);
    ~ModelTransformer();

    class Session* setupToSession(class Setup* s);
    void addGlobals(class MobiusConfig* config, class Session* session);
    void merge(class Setup* src, class Session* dest);

    void sessionToConfig(Session* src, MobiusConfig* dest);
    
  private:

    class Provider* provider = nullptr;
    class SymbolTable* symbols = nullptr;

    // values
    void transform(SymbolId id, const char* value, class ValueSet* dest);
    void transform(SymbolId id, int value, class ValueSet* dest);
    void transformBool(SymbolId id, bool value, class ValueSet* dest);
    void transformEnum(SymbolId id, int value, class ValueSet* dest);

    void transform(class MobiusConfig* src, class Session* dest);
    void transform(class Setup* src, class Session* dest);
    void transform(class Setup* setup, class SetupTrack* src, Session::Track* dest);

    const char* getString(SymbolId id, ValueSet* src);
    int getInt(SymbolId id, ValueSet* src);
    bool getBool(SymbolId id, ValueSet* src);
    void transform(Session* src, MobiusConfig* dest);
    void transform(Session* src, Setup* dest);
    void transform(Session::Track* src, SetupTrack* dest);
    
};
