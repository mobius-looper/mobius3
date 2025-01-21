/**
 * Utility to convert between the old MobiusConfig model and the new
 * Session model.
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
    
};
