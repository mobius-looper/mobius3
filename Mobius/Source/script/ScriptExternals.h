/**
 * Implementataions of external symbols provided only for the script environment.
 * Most externals correspond go Mobius core functions that are visible in bindings.
 * These are support functions just for script authors.
 */

#pragma once

class ScriptExternals
{
  public:

    ScriptExternals(class Supervisor* s) {
        supervisor = s;
    }
    ~ScriptExternals() {}

  private:

    class Supervisor* supervisor = nullptr;

};


    
