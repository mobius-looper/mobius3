
#pragma once

#include <stdio.h>

/****************************************************************************
 *                                                                          *
 *                              SCRIPT COMPILER                             *
 *                                                                          *
 ****************************************************************************/

#define SCRIPT_MAX_LINE 1024

/**
 * Parses script files and builds Script objects.
 * 
 * Encapsulates state necessary for script compilation.
 * This is used during both the parse and link phases.  
 *
 * During linking the script, lineNumber, and line fields are invalid.
 * The ExParser is always available.
 *
 * The compiler is normally built once when a ScriptConfig is loaded
 * and converted into a MScriptLibrary.  It may also be built to 
 * incrementally compile scripts that use the !autoload option.
 *
 */
class ScriptCompiler {
  public:

	ScriptCompiler();
	~ScriptCompiler();

    /**
     * Compile a ScriptConfig into a MScriptLibrary.
     */
    class MScriptLibrary* compile(class Mobius* m, class ScriptConfig* config);
    
    /**
     * Incrementally recompile one script.
     * This is what happens for !autoload.  
     */
    void recompile(class Mobius* m, class Script* script);

    // Utilities for the ScriptStatement constructors and linkers

    Mobius *getMobius();
    Script* getScript();
    char* skipToken(char* args, const char* token);
    class ExNode* parseExpression(class ScriptStatement* stmt, const char* src);
    class Script* resolveScript(const char* name);
    void syntaxError(class ScriptStatement* stmt, const char* msg);

  private:

    void parse(class ScriptRef* ref);
    bool parse(FILE* fp, class Script* script);
    void parseArgument(char* line, const char* keyword, char* buffer);
    class ScriptStatement* parseStatement(char* line);
    char* parseKeyword(char* line, char** retargs);
    void parseDeclaration(const char* line, const char* keyword);
    void link(class Script* s);
    Script* resolveScript(class Script* scripts, const char* name);

    /**
     * Supplies resolution for some references.
     */
    class Mobius* mMobius;

	/**
     * A parser for expressions, null if we're disabling expressions.
     */
	class ExParser* mParser;

    // Environment we're compiling into
    class MScriptLibrary* mLibrary;

    // scripts we have parsed
    class Script* mScripts;
    class Script* mLast;
    
	// the script we're currently parsing or linking
	class Script* mScript;

    // the script/proc block we're parsing
    class ScriptBlock* mBlock;

	// line number of the file we're currently parsing (0 during linking)
	int mLineNumber;

	// the unmodified line we're parsing
	char mLine[SCRIPT_MAX_LINE + 4];


};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

