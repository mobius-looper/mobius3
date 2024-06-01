/**
 * Simple model returned by the engine containing information
 * about the scripts referenced in a ScriptConfig.
 *
 */

#pragma once

class ScriptAnalysis
{
  public:

    ScriptAnalysis();
    ~ScriptAnalysis();

    // the names of all callable scripts
    // can be used to populate the potential target scripts in binding editors
    class StringList* getNames() {
        return mNames;
    }

    // error messages encounterd during analysis
    // needs evoluation, a simple list gets you there, but
    // a more complex structure that can be formatted nicely would be better
    class StringList* getErrors() {
        return mErrors;
    }

    // names of scripts that used the !button directive
    // to auto-populate UI action buttons
    class StringList* getButtons() {
        return mButtons;
    }

    // only for use by ScriptAnalyzer, which is not accessible at this level
    void addName(const char* name);
    void addError(const char* error);
    void addButton(const char* button);

  private:

    class StringList* mNames;
    class StringList* mErrors;
    class StringList* mButtons;

};

    
