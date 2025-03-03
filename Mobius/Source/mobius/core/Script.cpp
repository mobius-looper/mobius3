// commented out file handling, need to move this up

/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Data model, compiler and interpreter for a simple scripting language.
 *
 * We've grown a collection of "Script Internal Variables" that are
 * similar to Parameters.    A few things are represented in both
 * places (LoopFrames, LoopCycles).
 *
 * I'm leaning toward moving most of the read-only "track parameters"
 * from being ParameterDefs to script variables.  They're easier to 
 * maintain and they're really only for use in scripts anyway.
 * 
 * SCRIPT COMPILATION
 * 
 * Compilation of scripts proceeds in these phases.
 * 
 * Parse
 *   The script file is parsed and a Script object is constructed.
 *   Parsing is mostly carried out in the constructors for each 
 *   statement class.  Some statements may choose to parse their
 *   argument lists, others save the arguments for parsing during the
 *   Link phase.
 * 
 * Resolve
 *   References within the script are resolved.  This includes matching
 *   block start/end statements (if/endif, for/next) and locating
 *   referenced functions, variables, and parameters.
 * 
 * Link
 *   Call references between scripts in the MScriptLibrary are resolved.
 *   Some statements may do their expression parsing and variable
 *   resolution here too.  Included in this process is the construction
 *   of a new Function array including both static functions and scripts.
 * 
 * Export
 *   The new global Functions table built during the Link phase is installed.
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <ctype.h>

// for CD_SAMPLE_RATE, MSEC_TO_FRAMES
#include "AudioConstants.h"

#include "Expr.h"
#include "../../util/Trace.h"
#include "../../util/TraceClient.h"
#include "../../util/List.h"
#include "../../util/Util.h"

#include "../../model/ParameterConstants.h"
#include "../../model/old/Trigger.h"
#include "../../model/old/MobiusConfig.h"
#include "../../model/old/Setup.h"
#include "../../model/ScriptConfig.h"
#include "../../model/UserVariable.h"
#include "../../model/Symbol.h"
#include "../../model/old/Preset.h"
#include "../MobiusInterface.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
//#include "Export.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "Mode.h"
#include "Project.h"
#include "Script.h"
#include "ScriptInterpreter.h"
#include "Synchronizer.h"
#include "Track.h"
#include "Variable.h"

#include "ScriptCompiler.h"
#include "Script.h"
#include "Mem.h"

// for focus and groups
#include "../track/LogicalTrack.h"

/****************************************************************************
 *                                                                          *
 *   							  CONSTANTS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Notification labels.
 */
#define LABEL_REENTRY "reentry"
#define LABEL_SUSTAIN "sustain"
#define LABEL_END_SUSTAIN "endSustain"
#define LABEL_CLICK "click"
#define LABEL_END_CLICK "endClick"

/**
 * Default number of milliseconds in a "long press".
 */
#define DEFAULT_SUSTAIN_MSECS 200

/**
 * Default number of milliseconds we wait for a multi-click.
 */
#define DEFAULT_CLICK_MSECS 1000

/**
 * Names of wait types used in the script.  Order must
 * correspond to the WaitType enumeration.
 */
const char* WaitTypeNames[] = {
	"none",
	"last",
	"function",
	"event",
	"time",			// WAIT_RELATIVE
	"until",		// WAIT_ABSOLUTE
	"up",
	"long",
	"switch",
	"script",
	"block",
	"start",
	"end",
	"externalStart",
	"driftCheck",
	"pulse",
	"beat",
	"bar",
	"realign",
	"return",
	"thread",
	nullptr
};

/**
 * Names of wait unites used in the script. Order must correspond
 * to the WaitUnit enumeration.
 */
const char* WaitUnitNames[] = {
	"none",
	"msec",
	"frame",
	"subcycle",
	"cycle",
	"loop",
	nullptr
};

/****************************************************************************
 *                                                                          *
 *   							   RESOLVER                                 *
 *                                                                          *
 ****************************************************************************/

void ScriptResolver::init(ExSymbol* symbol)
{
	mSymbol = symbol;
    mStackArg = 0;
    mInternalVariable = nullptr;
    mVariable = nullptr;
    mParameterSymbol = nullptr;
}

ScriptResolver::ScriptResolver(ExSymbol* symbol, int arg)
{
	init(symbol);
    mStackArg = arg;
}

ScriptResolver::ScriptResolver(ExSymbol* symbol, ScriptInternalVariable* v)
{
	init(symbol);
    mInternalVariable = v;
}

ScriptResolver::ScriptResolver(ExSymbol* symbol, ScriptVariableStatement* v)
{
	init(symbol);
    mVariable = v;
}

ScriptResolver::ScriptResolver(ExSymbol* symbol, Symbol* s)
{
	init(symbol);
    mParameterSymbol = s;
}

ScriptResolver::ScriptResolver(ExSymbol* symbol, const char* name)
{
	init(symbol);
    mInterpreterVariable = name;
}

ScriptResolver::~ScriptResolver()
{
	// we don't own the symbol, it owns us
}

/**
 * Return the value of a resolved reference.
 * The ExContext passed here will be a ScriptInterpreter.
 */
void ScriptResolver::getExValue(ExContext* exContext, ExValue* value)
{
	// Here is the thing I hate about the interface.  We need to implement
	// a generic context, but when we eventually call back into ourselves
	// we have to downcast to our context.
	ScriptInterpreter* si = (ScriptInterpreter*)exContext;

	value->setNull();

    if (mStackArg > 0) {
		si->getStackArg(mStackArg, value);
    }
    else if (mInternalVariable != nullptr) {
		mInternalVariable->getValue(si, value);
    }
	else if (mVariable != nullptr) {
        UserVariables* vars = nullptr;
        const char* name = mVariable->getName();
		ScriptVariableScope scope = mVariable->getScope();
		switch (scope) {
			case SCRIPT_SCOPE_GLOBAL: {
				vars = si->getMobius()->getVariables();
			}
                break;
			case SCRIPT_SCOPE_TRACK: {
                vars = si->getTargetTrack()->getVariables();
			}
                break;
			default: {
				// maybe should be doing these on the ScriptStack instead?
                vars = si->getVariables();
			}
                break;
		}
        
        if (vars != nullptr)
          vars->get(name, value);
	}
    else if (mParameterSymbol != nullptr) {
        si->getMobius()->getParameter(mParameterSymbol, si->getTargetTrack(), value);
    }
    else if (mInterpreterVariable != nullptr) {
        UserVariables* vars = si->getVariables();
        if (vars != nullptr)
          vars->get(mInterpreterVariable, value);
    }
    else {
		// if it didn't resolve, we shouldn't have made it
		Trace(1, "ScriptResolver::getValue unresolved!\n");
	}
}

/****************************************************************************
 *                                                                          *
 *                                 ARGUMENTS                                *
 *                                                                          *
 ****************************************************************************/

ScriptArgument::ScriptArgument()
{
    mLiteral = nullptr;
    mStackArg = 0;
    mInternalVariable = nullptr;
    mVariable = nullptr;
    mParameterSymbol = nullptr;
}

const char* ScriptArgument::getLiteral()
{
    return mLiteral;
}

void ScriptArgument::setLiteral(const char* lit) 
{
	mLiteral = lit;
}

Symbol* ScriptArgument::getParameter()
{
    return mParameterSymbol;
}

bool ScriptArgument::isResolved()
{
	return (mStackArg > 0 ||
			mInternalVariable != nullptr ||
			mVariable != nullptr ||
			mParameterSymbol != nullptr);
}

/**
 * Script arguments may be literal values or references to stack arguments,
 * internal variables, local script variables, or parameters.
 * If it doesn't resolve it is left as a literal.
 */
void ScriptArgument::resolve(Mobius* m, ScriptBlock* block, 
                             const char* literal)
{
	mLiteral = literal;
    mStackArg = 0;
    mInternalVariable = nullptr;
    mVariable = nullptr;
    mParameterSymbol = nullptr;

    if (mLiteral != nullptr) {

		if (mLiteral[0] == '\'') {
			// kludge for a universal literal quoter until
			// we can figure out how to deal with parameter values
			// that are also the names of parameters, e.g. 
			// overdubMode=quantize
			mLiteral = &literal[1];
		}
		else {
			const char* ref = mLiteral;
			if (ref[0] == '$') {
				ref = &mLiteral[1];
				mStackArg = ToInt(ref);
			}
			if (mStackArg == 0) {

				mInternalVariable = ScriptInternalVariable::getVariable(ref);
				if (mInternalVariable == nullptr) {
                    if (block == nullptr)
                      Trace(1, "ScriptArgument::resolve has no block!\n");
                    else {
                        mVariable = block->findVariable(ref);
                        if (mVariable == nullptr) {
                            mParameterSymbol = m->findSymbol(ref);
                        }
                    }
                }
			}
		}
	}
}

/**
 * Retrieve the value of the argument to a buffer.
 *
 * !! This is exactly the same as ScriptResolver::getExValue, try to 
 * merge these.
 */
void ScriptArgument::get(ScriptInterpreter* si, ExValue* value)
{
	value->setNull();

    if (mStackArg > 0) {
		si->getStackArg(mStackArg, value);
    }
    else if (mInternalVariable != nullptr) {
        mInternalVariable->getValue(si, value);
    }
	else if (mVariable != nullptr) {
        UserVariables* vars = nullptr;
        const char* name = mVariable->getName();
		ScriptVariableScope scope = mVariable->getScope();
		switch (scope) {
			case SCRIPT_SCOPE_GLOBAL: {
				vars = si->getMobius()->getVariables();
			}
                break;
			case SCRIPT_SCOPE_TRACK: {
				vars = si->getTargetTrack()->getVariables();
			}
                break;
			default: {
				// maybe should be doing these on the ScriptStack instead?
				vars = si->getVariables();
			}
                break;
		}

        if (vars != nullptr)
          vars->get(name, value);
	}
    else if (mParameterSymbol != nullptr) {
        si->getMobius()->getParameter(mParameterSymbol, si->getTargetTrack(), value);
    }
    else if (mLiteral != nullptr) { 
		value->setString(mLiteral);
    }
    else {
		// This can happen for function statements with variable args
		// but is usually an error for other statement types
        //Trace(1, "Attempt to get invalid reference\n");
    }
}

/**
 * Assign a value through a reference.
 * Not all references are writable.
 */
void ScriptArgument::set(ScriptInterpreter* si, ExValue* value)
{
    if (mStackArg > 0) {
        // you can't set stack args
        Trace(1, "Script %s: Attempt to set script stack argument %s\n", 
              si->getTraceName(), mLiteral);
    }
	else if (mInternalVariable != nullptr) {
        const char* name = mInternalVariable->getName();
        char traceval[128];
        value->getString(traceval, sizeof(traceval));
        Trace(2, "Script %s: setting internal variable %s = %s\n", 
              si->getTraceName(), name, traceval);
		mInternalVariable->setValue(si, value);
    }
	else if (mVariable != nullptr) {
        char traceval[128];
        value->getString(traceval, sizeof(traceval));

        UserVariables* vars = nullptr;
        const char* name = mVariable->getName();
		ScriptVariableScope scope = mVariable->getScope();
        if (scope == SCRIPT_SCOPE_GLOBAL) {
			Trace(2, "Script %s: setting global variable %s = %s\n", 
                  si->getTraceName(), name, traceval);
            vars = si->getMobius()->getVariables();
        }
        else if (scope == SCRIPT_SCOPE_TRACK) {
			Trace(2, "Script %s: setting track variable %s = %s\n", 
                  si->getTraceName(), name, traceval);
			vars = si->getTargetTrack()->getVariables();
		}
		else {
			// maybe should be doing these on the ScriptStack instead?
			vars = si->getVariables();
		}
        
        if (vars != nullptr)
          vars->set(name, value);
	}
	else if (mParameterSymbol != nullptr) {
        si->getMobius()->setParameter(mParameterSymbol, si->getTargetTrack(), value);
	}
    else if (mLiteral != nullptr) {
        Trace(1, "Script %s: Attempt to set unresolved reference %s\n", 
              si->getTraceName(), mLiteral);
    }
    else {
        Trace(1, "Script %s: Attempt to set invalid reference\n",
              si->getTraceName());
    }

}


/****************************************************************************
 *                                                                          *
 *                                DECLARATION                               *
 *                                                                          *
 ****************************************************************************/

ScriptDeclaration::ScriptDeclaration(const char* name, const char* args)
{
    mNext = nullptr;
    mName = CopyString(name);
    mArgs = CopyString(args);
}

ScriptDeclaration::~ScriptDeclaration()
{
    delete mName;
    delete mArgs;
}

ScriptDeclaration* ScriptDeclaration::getNext()
{
    return mNext;
}

void ScriptDeclaration::setNext(ScriptDeclaration* next)
{
    mNext = next;
}

const char* ScriptDeclaration::getName()
{
    return mName;
}

const char* ScriptDeclaration::getArgs()
{
    return mArgs;
}

/****************************************************************************
 *                                                                          *
 *                                   BLOCK                                  *
 *                                                                          *
 ****************************************************************************/

ScriptBlock::ScriptBlock()
{
    mParent = nullptr;
    mName = nullptr;
    mDeclarations = nullptr;
	mStatements = nullptr;
	mLast = nullptr;
}

ScriptBlock::~ScriptBlock()
{
    // parent is not an ownership relationship, don't delete it

    delete mName;

	ScriptDeclaration* decl = nullptr;
	ScriptDeclaration* nextDecl = nullptr;
	for (decl = mDeclarations ; decl != nullptr ; decl = nextDecl) {
		nextDecl = decl->getNext();
		delete decl;
	}

	ScriptStatement* stmt = nullptr;
	ScriptStatement* nextStmt = nullptr;

	for (stmt = mStatements ; stmt != nullptr ; stmt = nextStmt) {
		nextStmt = stmt->getNext();
		delete stmt;
	}
}

ScriptBlock* ScriptBlock::getParent()
{
    return mParent;
}

void ScriptBlock::setParent(ScriptBlock* parent)
{
    mParent = parent;
}

const char* ScriptBlock::getName()
{
    return mName;
}

void ScriptBlock::setName(const char* name)
{

    delete mName;
    mName = CopyString(name);
}

ScriptDeclaration* ScriptBlock::getDeclarations()
{
    return mDeclarations;
}

void ScriptBlock::addDeclaration(ScriptDeclaration* decl)
{
    if (decl != nullptr) {
        // order doesn't matter
        decl->setNext(mDeclarations);
        mDeclarations = decl;
    }
}

ScriptStatement* ScriptBlock::getStatements()
{
	return mStatements;
}

void ScriptBlock::add(ScriptStatement* a)
{
	if (a != nullptr) {
		if (mLast == nullptr) {
			mStatements = a;
			mLast = a;
		}
		else {
			mLast->setNext(a);
			mLast = a;
		}

        if (a->getParentBlock() != nullptr)
          Trace(1, "ERROR: ScriptStatement already has a block!\n");
        a->setParentBlock(this);
	}
}

/**
 * Resolve referenes within the block.
 */
void ScriptBlock::resolve(Mobius* m)
{
    for (ScriptStatement* s = mStatements ; s != nullptr ; s = s->getNext())
      s->resolve(m);
}

/**
 * Resolve calls to other scripts within this block.
 */
void ScriptBlock::link(ScriptCompiler* comp)
{
    for (ScriptStatement* s = mStatements ; s != nullptr ; s = s->getNext())
	  s->link(comp);
}

/**
 * Search for a Variable declaration.
 * These are different than other block scoped things because
 * we also allow top-level script Variables to have global scope
 * within this script.  So if we don't find it within this block
 * we walk back up the block stack and look in the top block.
 * Intermediate blocks are not searched, if you want nested Procs
 * you need to pass arguments.  Could soften this?
 */
ScriptVariableStatement* ScriptBlock::findVariable(const char* name) {

    ScriptVariableStatement* found = nullptr;

    for (ScriptStatement* s = mStatements ; s != nullptr ; s = s->getNext()) {

        if (s->isVariable()) {
            ScriptVariableStatement* v = (ScriptVariableStatement*)s;
            if (name == nullptr || StringEqualNoCase(name, v->getName())) {
                found = v;
                break;
            }
        }
    }

    if (found == nullptr) {
        ScriptBlock* top = mParent;
        while (top != nullptr && top->getParent() != nullptr)
          top = top->getParent();

        if (top != nullptr)
          found = top->findVariable(name);
    }

    return found;
}

/**
 * Search for a Label statement.
 */
ScriptLabelStatement* ScriptBlock::findLabel(const char* name)
{
	ScriptLabelStatement* found = nullptr;

	for (ScriptStatement* s = mStatements ; s != nullptr ; s = s->getNext()) {
		if (s->isLabel()) {
			ScriptLabelStatement* l = (ScriptLabelStatement*)s;
			if (name == nullptr || StringEqualNoCase(name, l->getArg(0))) {
                found = l;
                break;
            }
        }
    }
	return found;
}

/**
 * Search for a Proc statement.
 * These are like Variables, we can local Procs in the block (rare)
 * or script-global procs.
 */
ScriptProcStatement* ScriptBlock::findProc(const char* name)
{
	ScriptProcStatement* found = nullptr;

	for (ScriptStatement* s = mStatements ; s != nullptr ; s = s->getNext()) {
		if (s->isProc()) {
			ScriptProcStatement* p = (ScriptProcStatement*)s;
			if (name == nullptr || StringEqualNoCase(name, p->getArg(0))) {
                found = p;
                break;
            }
        }
    }

    if (found == nullptr) {
        ScriptBlock* top = mParent;
        while (top != nullptr && top->getParent() != nullptr)
          top = top->getParent();

        if (top != nullptr)
          found = top->findProc(name);
    }

	return found;
}

/**
 * Search for the For/Repeat statement matching a Next.
 */
ScriptIteratorStatement* ScriptBlock::findIterator(ScriptNextStatement* next)
{
    ScriptIteratorStatement* found = nullptr;

    for (ScriptStatement* s = mStatements ; s != nullptr ; s = s->getNext()) {
		// loops can be nested so find the nearest one that isn't already
		// paired with a next statement
        if (s->isIterator() && ((ScriptIteratorStatement*)s)->getEnd() == nullptr)
		  found = (ScriptIteratorStatement*)s;
        else if (s == next)
		  break;
    }
    return found;
}

/**
 * Search for the statement ending an if/else clause.  Arguent may
 * be either an If or Else statement.  Return value will be either
 * an Else or Endif statement.
 */
ScriptStatement* ScriptBlock::findElse(ScriptStatement* start)
{
	ScriptStatement* found = nullptr;
	int depth = 0;

	for (ScriptStatement* s = start->getNext() ; s != nullptr && found == nullptr ;
		 s = s->getNext()) {

		// test isElse first since isIf will also true
		if (s->isElse()) {
			if (depth == 0)
			  found = s;	
		}
		else if (s->isIf()) {
			depth++;
		}
		else if (s->isEndif()) {
			if (depth == 0)
			  found = s;
			else
			  depth--;
		}
	}

	return found;
}

/****************************************************************************
 *                                                                          *
 *   							  STATEMENT                                 *
 *                                                                          *
 ****************************************************************************/

ScriptStatement::ScriptStatement()
{
    mParentBlock = nullptr;
	mNext = nullptr;
	for (int i = 0 ; i < MAX_ARGS ; i++)
	  mArgs[i] = nullptr;
}

ScriptStatement::~ScriptStatement()
{
	for (int i = 0 ; i < MAX_ARGS ; i++)
	  delete mArgs[i];
}

void ScriptStatement::setParentBlock(ScriptBlock* b)
{
    if (b == (ScriptBlock*)this)
      Trace(1, "ScriptStatement::setBlock circular reference!\n");
    else
      mParentBlock = b;
}

ScriptBlock* ScriptStatement::getParentBlock()
{
	return mParentBlock;
}

void ScriptStatement::setNext(ScriptStatement* a)
{
	mNext = a;
}

ScriptStatement* ScriptStatement::getNext()
{
	return mNext;
}

void ScriptStatement::setArg(const char* arg, int psn)
{
	delete mArgs[psn];
	mArgs[psn] = nullptr;
	if (arg != nullptr && strlen(arg) > 0)
	  mArgs[psn] = CopyString(arg);
}

const char* ScriptStatement::getArg(int psn)
{
	return mArgs[psn];
}

void ScriptStatement::setLineNumber(int i)
{
    mLineNumber = i;
}

int ScriptStatement::getLineNumber()
{
    return mLineNumber;
}

bool ScriptStatement::isVariable()
{
    return false;
}

bool ScriptStatement::isLabel()
{
    return false;
}

bool ScriptStatement::isIterator()
{
    return false;
}

bool ScriptStatement::isNext()
{
    return false;
}

bool ScriptStatement::isEnd()
{
    return false;
}

bool ScriptStatement::isBlock()
{
    return false;
}

bool ScriptStatement::isProc()
{
    return false;
}

bool ScriptStatement::isEndproc()
{
    return false;
}

bool ScriptStatement::isParam()
{
    return false;
}

bool ScriptStatement::isEndparam()
{
    return false;
}

bool ScriptStatement::isIf()
{
    return false;
}

bool ScriptStatement::isElse()
{
    return false;
}

bool ScriptStatement::isEndif()
{
    return false;
}

bool ScriptStatement::isFor()
{
    return false;
}

/**
 * Called after the script has been fully parsed.
 * Overloaded by the subclasses to resolve references to 
 * things within the script such as matching block statements
 * (if/endif, for/next) and variables.
 */
void ScriptStatement::resolve(Mobius* m)
{
    (void)m;
}

/**
 * Called when the entire MScriptLibrary has been loaded and the
 * scripts have been exported to the global function table.
 * Overloaded by the subclasses to resolve references between scripts.
 */
void ScriptStatement::link(ScriptCompiler* compiler)
{
    (void)compiler;
}

/**
 * Serialize a statement.  Assuming we can just emit the original
 * arguments, don't need to noramlize.
 */
#if 0
void ScriptStatement::xwrite(FILE* fp)
{
    fprintf(fp, "%s", getKeyword());

	for (int i = 0 ; mArgs[i] != nullptr ; i++) 
      fprintf(fp, " %s", mArgs[i]);

	fprintf(fp, "\n");
}
#endif

/**
 * Parse the remainder of the function line into up to 8 arguments.
 */
void ScriptStatement::parseArgs(char* line)
{
	parseArgs(line, 0, 0);
}

/**
 * Clear any previously parsed arguments.
 * Necessary to prevent leaks for the few functions that
 * parse more than once.
 *
 * This is a mess, I'd like to say that if you call parseArgs
 * more than once, then you lose any char pointers to things
 * previously parsed but I'm concerned about statements wanting
 * to keep the previous values and not expecting secondary parseArgs
 * from deleting them.   Complex parsing is mostly in Wait and Function.
 * Calling this makes it explicit that you expect to lose previously
 * parsed values
 */
void ScriptStatement::clearArgs()
{
	for (int i = 0 ; i < MAX_ARGS ; i++) {
        delete mArgs[i];
        mArgs[i] = nullptr;
    }
}

/**
 * Parse the remainder of the function line into up to 8 arguments.
 * If a maximum argument count is given, return the remainder of the
 * line after that number of arguments has been located.
 *
 * Technically this should probably go in ScriptCompiler but it's
 * easier to use if we have it here.
 */
char* ScriptStatement::parseArgs(char* line, int argOffset, int toParse)
{
	if (line != nullptr) {

        int max = MAX_ARGS;
        if (toParse > 0) {
            max = argOffset + toParse;
			if (max > MAX_ARGS)
              max = MAX_ARGS;
		}

		while (*line && argOffset < max) {
			bool quoted = false;

			// skip preceeding whitespace
			while (*line && IsSpace(*line)) line++;

			if (*line == '"') {
				quoted = true;
				line++;
			}

			if (*line) {
				char* token = line;
				if (quoted)
				  while (*line && *line != '"') line++;
				else
				  while (*line && !IsSpace(*line)) line++;
			
				bool more = (*line != 0);
				*line = 0;
				if (strlen(token)) {

                    // !! for a few statements this may have prior content
                    // make the caller clean this out until we can figure
                    // out the best way to safely reclaim these
                    // won't be here if clearArgs() was called
                    if (mArgs[argOffset] != nullptr)
                      Trace(1, "ScriptStatemement::parseArgs lingering argument value from prior parse!!!!!!!!\n");
                    mArgs[argOffset] = CopyString(token);
                    argOffset++;
				}
				if (more)
				  line++;
			}
		}
	}

	return line;
}

/****************************************************************************
 *                                                                          *
 *                                    ECHO                                  *
 *                                                                          *
 ****************************************************************************/

ScriptEchoStatement::ScriptEchoStatement(ScriptCompiler* comp,
                                         char* args)
{
    (void)comp;
    // unlike most other functions, this one doesn't tokenize args
    setArg(args, 0);
}

const char* ScriptEchoStatement::getKeyword()
{
    return "Echo";
}

ScriptStatement* ScriptEchoStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

    si->expand(mArgs[0], &v);

	char* msg = v.getBuffer();

	// add a newline so we can use it with OutputDebugStream
	// note that the buffer has extra padding on the end for the nul
	int len = (int)strlen(msg);
	if (len < MAX_ARG_VALUE) {
		msg[len] = '\n';
		msg[len+1] = 0;
	}

    // The main use of this is to send messages to the trace log for debugging
    Trace(msg);

    // TestDriver also wants to intercept these to display in the summary tab
    // todo: Some fast tests like exprtest send enough KernelEcho messages
    // that they overflow the KernelCommunicator message buffer and
    // cause an ERROR in the trace log.  This doesn't hurt anything but
    // it looks alarming in the test log.  Consider a special test mode
    // that bypasses KernelCommunicator and directly queues Echo messages
    // in TestDriver 
    si->sendKernelEvent(EventEcho, msg);
    
    return nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// TestStart
//
//////////////////////////////////////////////////////////////////////

ScriptTestStartStatement::ScriptTestStartStatement(ScriptCompiler* comp,
                                                   char* args)
{
    (void)comp;
    // unlike most other functions, this one doesn't tokenize args
    setArg(args, 0);
}

const char* ScriptTestStartStatement::getKeyword()
{
    return "TestStart";
}

ScriptStatement* ScriptTestStartStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

    si->expand(mArgs[0], &v);

	char* msg = v.getBuffer();

    Trace(2, "TestStart ******************  %s  ***************\n", msg);

    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *   							   MESSAGE                                  *
 *                                                                          *
 ****************************************************************************/

// We now have two ways to send text to the UI, as an "alert"
// and as a "message".  Messages had a statement dedicated to it.
// Alert has a function.  Don't remember why there was a difference.

ScriptMessageStatement::ScriptMessageStatement(ScriptCompiler* comp,
                                               char* args)
{
    (void)comp;
    // unlike most other functions, this one doesn't tokenize args
    setArg(args, 0);
}

const char* ScriptMessageStatement::getKeyword()
{
    return "Message";
}

ScriptStatement* ScriptMessageStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

    // !! should be using KernelEvent if we need this at all,
    // can't just be calling a UI listener in the audio thread
    si->expand(mArgs[0], &v);
	char* msg = v.getBuffer();

    Trace(3, "Script %s: message %s\n", si->getTraceName(), msg);

	Mobius* m = si->getMobius();
    m->sendMobiusMessage(msg);

    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *   								PROMPT                                  *
 *                                                                          *
 ****************************************************************************/

ScriptPromptStatement::ScriptPromptStatement(ScriptCompiler* comp,
                                             char* args)
{
    (void)comp;
    // like echo, we'll assume that the remainder is the message
	// probably want to change this to support button configs?
    setArg(args, 0);
}

const char* ScriptPromptStatement::getKeyword()
{
    return "Prompt";
}

ScriptStatement* ScriptPromptStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

    si->expand(mArgs[0], &v);
	char* msg = v.getBuffer();

    si->sendKernelEvent(EventPrompt, msg);

	// we always automatically wait for this
	si->setupWaitThread(this);

    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                    END                                   *
 *                                                                          *
 ****************************************************************************/

ScriptEndStatement ScriptEndStatementObj {nullptr, nullptr};
ScriptEndStatement* ScriptEndStatement::Pseudo = &ScriptEndStatementObj;

ScriptEndStatement::ScriptEndStatement(ScriptCompiler* comp,
                                       char* args)
{
    (void)comp;
    (void)args;
}

const char* ScriptEndStatement::getKeyword()
{
    return "End";
}

bool ScriptEndStatement::isEnd()
{
    return true;
}

ScriptStatement* ScriptEndStatement::eval(ScriptInterpreter* si)
{
    Trace(2, "Script %s: end\n", si->getTraceName());
    return nullptr;
}


/****************************************************************************
 *                                                                          *
 *   								CANCEL                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Currently intended for use only in async notification threads, though
 * think more about this, could be used to cancel an iteration?
 *
 *    Cancel for, while, repeat
 *    Cancel loop
 *    Cancel iteration
 *    Break
 */
ScriptCancelStatement::ScriptCancelStatement(ScriptCompiler* comp,
                                             char* args)
{
    (void)comp;
    parseArgs(args);
	mCancelWait = StringEqualNoCase(mArgs[0], "wait");
}

const char* ScriptCancelStatement::getKeyword()
{
    return "Cancel";
}

ScriptStatement* ScriptCancelStatement::eval(ScriptInterpreter* si)
{
	ScriptStatement* next = nullptr;

    Trace(2, "Script %s: cancel\n", si->getTraceName());

	if (mCancelWait) {
		// This only makes sense within a notification thread, in the main
		// thread we couldn't be in a wait state
		// !! Should we set a script local variable that can be tested
		// to tell if this happened?
		ScriptStack* stack = si->getStack();
		if (stack != nullptr)
		  stack->cancelWaits();
	}
	else {
		// Cancel the entire script
		// I suppose it is ok to call this in the main thread, it will
		// behave like end
		si->reset();
		next = ScriptEndStatement::Pseudo;
	}

    return next;
}

/****************************************************************************
 *                                                                          *
 *                                 INTERRUPT                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Alternative to Cancel that can interrupt other scripts.
 * With no argument it breaks out of a  Wait in this thread.  With an 
 * argument it attempts to find a thread running a script with that name
 * and cancels it.
 *
 * TODO: Might be nice to set a variable in the target script:
 *
 *     Interrrupt MyScript varname foo
 *
 * But then we'll have to treat the script name as a single string constant
 * if it has spaces:
 *
 *     Interrupt "Some Script" varname foo
 *
 */
ScriptInterruptStatement::ScriptInterruptStatement(ScriptCompiler* comp, 
                                                   char* args)
{
    (void)comp;
    (void)args;
}

const char* ScriptInterruptStatement::getKeyword()
{
    return "Interrupt";
}

ScriptStatement* ScriptInterruptStatement::eval(ScriptInterpreter* si)
{
    Trace(3, "Script %s: interrupt\n", si->getTraceName());

    ScriptStack* stack = si->getStack();
    if (stack != nullptr)
      stack->cancelWaits();

    // will this work without a declaration?
    UserVariables* vars = si->getVariables();
    if (vars != nullptr) {
        ExValue v;
        v.setString("true");
        vars->set("interrupted", &v);
    }

    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                    SET                                   *
 *                                                                          *
 ****************************************************************************/

ScriptSetStatement::ScriptSetStatement(ScriptCompiler* comp, 
                                       char* args)
{
    (void)comp;
	mExpression = nullptr;

	// isolate the first argument representing the reference
	// to the thing to set, the remainder is an expression
	args = parseArgs(args, 0, 1);

    if (args == nullptr) {
        Trace(1, "Malformed set statement, missing arguments\n");
    }
    else {
        // ignore = between the name and initializer
        char* ptr = args;
        while (*ptr && IsSpace(*ptr)) ptr++;
        if (*ptr == '=') 
          args = ptr + 1;

        // defer this to link?
        mExpression = comp->parseExpression(this, args);
    }
}

ScriptSetStatement::~ScriptSetStatement()
{
	delete mExpression;
}

const char* ScriptSetStatement::getKeyword()
{
    return "Set";
}

void ScriptSetStatement::resolve(Mobius* m)
{
    mName.resolve(m, mParentBlock, mArgs[0]);
}

ScriptStatement* ScriptSetStatement::eval(ScriptInterpreter* si)
{
	if (mExpression != nullptr) {
		ExValue v;
		mExpression->eval(si, &v);
		mName.set(si, &v);
	}
	return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                    USE                                   *
 *                                                                          *
 ****************************************************************************/

ScriptUseStatement::ScriptUseStatement(ScriptCompiler* comp, 
                                       char* args) :
    ScriptSetStatement(comp, args)
    
{
    (void)comp;
    (void)args;
}

const char* ScriptUseStatement::getKeyword()
{
    return "Use";
}

ScriptStatement* ScriptUseStatement::eval(ScriptInterpreter* si)
{
    Trace(1, "ScriptUseStatement: No longer implemented");
    (void)si;
#if 0    
    Parameter* p = mName.getParameter();
    if (p == nullptr) {
        Trace(1, "ScriptUseStatement: Not a parameter: %s\n", 
              mName.getLiteral());
    }   
    else { 
        si->use(p);
    }

    return ScriptSetStatement::eval(si);
#endif
    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                  VARIABLE                                *
 *                                                                          *
 ****************************************************************************/

ScriptVariableStatement::ScriptVariableStatement(ScriptCompiler* comp,
                                                 char* args)
{
	mScope = SCRIPT_SCOPE_SCRIPT;
	mName = nullptr;
	mExpression = nullptr;

	// isolate the scope identifier and variable name

    // new: arg parsing is WAY to fucking memory sensitive
    // this is a weird statement parser because normally mArgs
    // has string copies that get left behind and deleted
    // in the ScriptStatement destructor
    // here we're parsing args twice, once to look for the
    // scope arg which we convert into a constant, and then
    // again for the rest
    // since parseArgs doesn't delete prior content we'll
    // leak whatever was in mArgs[0]
    // so you could have parseArgs detect that, but I'm afraid if
    // it deletes it, something could have captured the value
    // like we do here with mName and ownership transfer was not
    // obvious
    // to prevent the leak, "steal" it from the mArgs array and
    // put a trace in parseArgs when it notices prior content so we
    // can hunt those down
    
	args = parseArgs(args, 0, 1);
	const char* arg = mArgs[0];

	if (StringEqualNoCase(arg, "global"))
      mScope = SCRIPT_SCOPE_GLOBAL;
	else if (StringEqualNoCase(arg, "track"))
      mScope = SCRIPT_SCOPE_TRACK;
	else if (StringEqualNoCase(arg, "script"))
      mScope = SCRIPT_SCOPE_SCRIPT;
	else {
		// if not one of the keywords assume the name
		mName = arg;
	}

	if (mName == nullptr) {
		// first arg was the scope, parse another
        // see comments above about what we're doing here
        delete mArgs[0];
        mArgs[0] = nullptr;
        
		args = parseArgs(args, 0, 1);
		mName = mArgs[0];
	}

	// ignore = between the name and initializer
    if (args == nullptr) {
        Trace(1, "Malformed Variable statement: missing arguments\n");
    }
    else {
        char* ptr = args;
        while (*ptr && IsSpace(*ptr)) ptr++;
        if (*ptr == '=') 
          args = ptr + 1;

        // the remainder is the initialization expression

        mExpression = comp->parseExpression(this, args);
    }
}

ScriptVariableStatement::~ScriptVariableStatement()
{
	delete mExpression;
}

const char* ScriptVariableStatement::getKeyword()
{
    return "Variable";
}

bool ScriptVariableStatement::isVariable()
{
    return true;
}

const char* ScriptVariableStatement::getName()
{
	return mName;
}

ScriptVariableScope ScriptVariableStatement::getScope()
{
	return mScope;
}

/**
 * These will have the side effect of initializing the variable, depending
 * on the scope.  For variables in global and track scope, the initialization
 * expression if any is run only if there is a null value.  For script scope
 * the initialization expression is run every time.
 *
 * Hmm, if we run global/track expressions on non-null it means
 * that we can never set a global to null. 
 */
ScriptStatement* ScriptVariableStatement::eval(ScriptInterpreter* si)
{
    Trace(3, "Script %s: Variable %s\n", si->getTraceName(), mName);

	if (mName != nullptr && mExpression != nullptr) {
        
        UserVariables* vars = nullptr;
        // sigh, don't have a trace sig that takes 3 strings 
        const char* tracemsg = nullptr;

		switch (mScope) {
			case SCRIPT_SCOPE_GLOBAL: {
                vars = si->getMobius()->getVariables();
                tracemsg = "Script %s: initializing global variable %s = %s\n";
			}
                break;
			case SCRIPT_SCOPE_TRACK: {
                vars = si->getTargetTrack()->getVariables();
                tracemsg = "Script %s: initializing track variable %s = %s\n";
            }
                break;
			case SCRIPT_SCOPE_SCRIPT: {
                vars = si->getVariables();
                tracemsg = "Script %s: initializing script variable %s = %s\n";
			}
                break;
		}

        if (vars == nullptr)  {
            Trace(1, "Script %s: Invalid variable scope!\n", si->getTraceName());
        }
        else if (mScope == SCRIPT_SCOPE_SCRIPT || !vars->isBound(mName)) {
            // script scope vars always initialize
            ExValue value;
            mExpression->eval(si, &value);

            Trace(2, tracemsg, si->getTraceName(), mName, value.getString());
            vars->set(mName, &value);
        }
	}

    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *   							 CONDITIONAL                                *
 *                                                                          *
 ****************************************************************************/

ScriptConditionalStatement::ScriptConditionalStatement()
{
	mCondition = nullptr;
}

ScriptConditionalStatement::~ScriptConditionalStatement()
{
	delete mCondition;
}

bool ScriptConditionalStatement::evalCondition(ScriptInterpreter* si)
{
	bool value = false;

	if (mCondition != nullptr) {
		value = mCondition->evalToBool(si);
	}
	else {
		// unconditional
		value = true;
	}

	return value;
}

/****************************************************************************
 *                                                                          *
 *                                    JUMP                                  *
 *                                                                          *
 ****************************************************************************/

ScriptJumpStatement::ScriptJumpStatement(ScriptCompiler* comp, 
                                         char* args)
{
    (void)comp;
    mStaticLabel = nullptr;

	// the label
	args = parseArgs(args, 0, 1);
    
    if (args == nullptr) {
        Trace(1, "Malformed Jump statement: missing arguments\n");
    }
    else {
        // then the condition
        mCondition = comp->parseExpression(this, args);
    }
}

const char* ScriptJumpStatement::getKeyword()
{
    return "Jump";
}

void ScriptJumpStatement::resolve(Mobius* m)
{
    // try to resolve it to to a variable or stack arg for dynamic
    // jump labels
    mLabel.resolve(m, mParentBlock, mArgs[0]);
    if (!mLabel.isResolved()) {

        // a normal literal reference, try to find it now
        mStaticLabel = mParentBlock->findLabel(mLabel.getLiteral());
    }
}

ScriptStatement* ScriptJumpStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = nullptr;
	ExValue v;

	mLabel.get(si, &v);
	const char* label = v.getString();

	Trace(3, "Script %s: Jump %s\n", si->getTraceName(), label);

	if (evalCondition(si)) {
		if (mStaticLabel != nullptr)
		  next = mStaticLabel;
		else {
			// dynamic resolution
            if (mParentBlock != nullptr)
              next = mParentBlock->findLabel(label);

			if (next == nullptr) {
				// halt when this happens or ignore?
				Trace(1, "Script %s: unresolved jump label %s\n", 
                      si->getTraceName(), label);
			}
		}
    }

    return next;
}

/****************************************************************************
 *                                                                          *
 *   							IF/ELSE/ENDIF                               *
 *                                                                          *
 ****************************************************************************/

ScriptIfStatement::ScriptIfStatement(ScriptCompiler* comp, 
                                     char* args)
{
    (void)comp;
    mElse = nullptr;

	// ignore the first token if it is "if", it is a common error to
	// use "else if" rather than "else if"
	if (args != nullptr) {
		while (*args && IsSpace(*args)) args++;
		if (StartsWithNoCase(args, "if "))
		  args += 3;
	}

	mCondition = comp->parseExpression(this, args);
}

const char* ScriptIfStatement::getKeyword()
{
    return "If";
}

bool ScriptIfStatement::isIf()
{
	return true;
}

ScriptStatement* ScriptIfStatement::getElse()
{
	return mElse;
}

void ScriptIfStatement::resolve(Mobius* m)
{
    (void)m;
    // search for matching else/elseif/endif
    mElse = mParentBlock->findElse(this);
}

ScriptStatement* ScriptIfStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = nullptr;
	ScriptIfStatement* clause = this;

	Trace(3, "Script %s: %s\n", si->getTraceName(), getKeyword());

	if (isElse()) {
		// Else conditionals are processed by the original If statement,
		// if we get here, we're skipping over the other clauses after
		// one of them has finished
		next = mElse;
	}
	else {
		// keep jumping through clauses until we can enter one
		while (next == nullptr && clause != nullptr) {
			if (clause->evalCondition(si)) {
				next = clause->getNext();
				if (next == nullptr) {
					// malformed, don't infinite loop
                    Trace(1, "Script %s: ScriptIfStatement: malformed clause\n", si->getTraceName());
					next = ScriptEndStatement::Pseudo;
				}
			}
			else {
				ScriptStatement* nextClause = clause->getElse();
				if (nextClause == nullptr) {
					// malformed
                    Trace(1, "Script %s: ScriptIfStatement: else or missing endif\n", si->getTraceName());
					next = ScriptEndStatement::Pseudo;
				}
				else if (nextClause->isIf()) {
					// try this one
					clause = (ScriptIfStatement*)nextClause;
				}
				else {
					// must be an endif
					next = nextClause;
				}
			}
		}
	}

    return next;
}

ScriptElseStatement::ScriptElseStatement(ScriptCompiler* comp, 
                                         char* args) : 
	ScriptIfStatement(comp, args)
{
    (void)comp;
    (void)args;
}

const char* ScriptElseStatement::getKeyword()
{
    return (mCondition != nullptr) ? "Elseif" : "Else";
}

bool ScriptElseStatement::isElse()
{
	return true;
}

ScriptEndifStatement::ScriptEndifStatement(ScriptCompiler* comp, 
                                           char* args)
{
    (void)comp;
    (void)args;
}

const char* ScriptEndifStatement::getKeyword()
{
    return "Endif";
}

bool ScriptEndifStatement::isEndif()
{
	return true;
}

/**
 * When we finally get here, just go to the next one after it.
 */
ScriptStatement* ScriptEndifStatement::eval(ScriptInterpreter* si)
{
    (void)si;
	return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                   LABEL                                  *
 *                                                                          *
 ****************************************************************************/

ScriptLabelStatement::ScriptLabelStatement(ScriptCompiler* comp, 
                                           char* args)
{
    (void)comp;
    parseArgs(args);
}

const char* ScriptLabelStatement::getKeyword()
{
    return "Label";
}

bool ScriptLabelStatement::isLabel()
{   
    return true;
}

bool ScriptLabelStatement::isLabel(const char* name)
{
	return StringEqualNoCase(name, getArg(0));
}

ScriptStatement* ScriptLabelStatement::eval(ScriptInterpreter* si)
{
    (void)si;
    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *   							   ITERATOR                                 *
 *                                                                          *
 ****************************************************************************/

ScriptIteratorStatement::ScriptIteratorStatement()
{
	mEnd = nullptr;
	mExpression = nullptr;
}

ScriptIteratorStatement::~ScriptIteratorStatement()
{
	delete mExpression;
}

/**
 * Rather than try to resolve the corresponding Next statement, let
 * the Next resolve find us.
 */
void ScriptIteratorStatement::setEnd(ScriptNextStatement* next)
{
	mEnd = next;
}

ScriptNextStatement* ScriptIteratorStatement::getEnd()
{
	return mEnd;
}

bool ScriptIteratorStatement::isIterator()
{
	return true;
}

/****************************************************************************
 *                                                                          *
 *                                    FOR                                   *
 *                                                                          *
 ****************************************************************************/

ScriptForStatement::ScriptForStatement(ScriptCompiler* comp, 
                                       char* args)
{
    (void)comp;
	// there is only oen arg, let it have spaces
	// !!! support expressions?
    setArg(args, 0);
}

const char* ScriptForStatement::getKeyword()
{
    return "For";
}

bool ScriptForStatement::isFor()
{
	return true;
}

/**
 * Initialize the track target list for a FOR statement.
 * There can only be one of these active a time (no nesting).
 * If you try that, the second one takes over and the outer one
 * will complete.
 *
 * To support nesting iteratation state is maintained on a special
 * stack frame to represent a "block" rather than a call.
 */
ScriptStatement* ScriptForStatement::eval(ScriptInterpreter* si)
{
	ScriptStatement* next = nullptr;
    Mobius* m = si->getMobius();
	int trackCount = m->getTrackCount();
	ExValue v;

	// push a block frame to hold iteration state
	ScriptStack* stack = si->pushStack(this);

	// this one needs to be recursively expanded at runtime
	si->expand(mArgs[0], &v);
	const char* forspec = v.getString();

	Trace(3, "Script %s: For %s\n", si->getTraceName(), forspec);
	// it's a common error to have trailing spaces so use StartsWith
	if (strlen(forspec) == 0 ||
        StartsWithNoCase(forspec, "all") ||
        StartsWithNoCase(forspec, "*")) {

		for (int i = 0 ; i < trackCount ; i++)
		  stack->addTrack(m->getTrack(i));
	}
	else if (StartsWithNoCase(forspec, "focused")) {
		for (int i = 0 ; i < trackCount ; i++) {
			Track* t = m->getTrack(i);
            LogicalTrack* lt = t->getLogicalTrack();
			if (lt->isFocused() || t == m->getTrack())
			  stack->addTrack(t);
		}
	}
	else if (StartsWithNoCase(forspec, "muted")) {
		for (int i = 0 ; i < trackCount ; i++) {
			Track* t = m->getTrack(i);
			Loop* l = t->getLoop();
			if (l->isMuteMode())
			  stack->addTrack(t);
		}
	}
	else if (StartsWithNoCase(forspec, "playing")) {
		for (int i = 0 ; i < trackCount ; i++) {
			Track* t = m->getTrack(i);
			Loop* l = t->getLoop();
			if (!l->isReset() && !l->isMuteMode())
			  stack->addTrack(t);
		}
	}
	else if (StartsWithNoCase(forspec, "group")) {
		int group = ToInt(&forspec[5]);
		if (group > 0) {
			// assume for now that tracks can't be in more than one group
			// could do that with a bit mask if necessary
			for (int i = 0 ; i < trackCount ; i++) {
				Track* t = m->getTrack(i);
                LogicalTrack* lt = t->getLogicalTrack();
				if (lt->getGroup() == group)
				  stack->addTrack(t);
			}
		}
	}
	else if (StartsWithNoCase(forspec, "outSyncMaster")) {
        Synchronizer* sync = m->getSynchronizer();
        Track* t = sync->getOutSyncMaster();
        if (t != nullptr)
          stack->addTrack(t);
	}
	else if (StartsWithNoCase(forspec, "trackSyncMaster")) {
        Synchronizer* sync = m->getSynchronizer();
        Track* t = sync->getTrackSyncMaster();
        if (t != nullptr)
          stack->addTrack(t);
	}
	else {
		char number[MIN_ARG_VALUE];
		int max = (int)strlen(forspec);
		int digits = 0;

		for (int i = 0 ; i <= max ; i++) {
			char ch = forspec[i];
			if (ch != 0 && isdigit(ch))
			  number[digits++] = ch;
			else if (digits > 0) {
				number[digits] = 0;
				int tracknum = ToInt(number) - 1;
				Track* t = m->getTrack(tracknum);
				if (t != nullptr)
				  stack->addTrack(t);
				digits = 0;
			}
		}
	}

	// if nothing was added, then skip it
	if (stack->getMax() == 0) {
		si->popStack();
		if (mEnd != nullptr)
		  next = mEnd->getNext();

		if (next == nullptr) {
			// at the end of the script
			// returning null means go to OUR next statement, here
			// we need to return the pseudo End statement to make
			// this script terminate
			next = ScriptEndStatement::Pseudo;
		}
	}

    return next;
}

/**
 * Called by the ScriptNextStatement evaluator.  
 * Advance to the next track if we can.
 */
bool ScriptForStatement::isDone(ScriptInterpreter* si)
{
	bool done = false;

	ScriptStack* stack = si->getStack();

	if (stack == nullptr) {
		Trace(1, "Script %s: For lost iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else if (stack->getIterator() != this) {
		Trace(1, "Script %s: For mismatched iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else {
		Track* nextTrack = stack->nextTrack();
		if (nextTrack != nullptr)
		  Trace(3, "Script %s: For track %ld\n", 
                si->getTraceName(), (long)nextTrack->getDisplayNumber());
		else {
			Trace(3, "Script %s: end of For\n", si->getTraceName());
			done = true;
		}
	}

    return done;
}

/****************************************************************************
 *                                                                          *
 *   								REPEAT                                  *
 *                                                                          *
 ****************************************************************************/

ScriptRepeatStatement::ScriptRepeatStatement(ScriptCompiler* comp, 
                                             char* args)
{
    (void)comp;
	mExpression = comp->parseExpression(this, args);
}

const char* ScriptRepeatStatement::getKeyword()
{
    return "Repeat";
}

/**
 * Assume for now that we an only specify a number of repetitions
 * e.g. "Repeat 2" for 2 repeats.  Eventually could have more flexible
 * iteration ranges like "Repeat 4 8" meaning iterate from 4 to 8 by 1, 
 * but I can't see a need for that yet.
 */
ScriptStatement* ScriptRepeatStatement::eval(ScriptInterpreter* si)
{
	ScriptStatement* next = nullptr;
	char spec[MIN_ARG_VALUE + 4];
	strcpy(spec, "");

	if (mExpression != nullptr)
	  mExpression->evalToString(si, spec, MIN_ARG_VALUE);

	Trace(3, "Script %s: Repeat %s\n", si->getTraceName(), spec);

	int count = ToInt(spec);
	if (count > 0) {
		// push a block frame to hold iteration state
		ScriptStack* stack = si->pushStack(this);
		stack->setMax(count);
	}
	else {
		// Invalid repetition count or unresolved variable, treat
		// this like an If with a false condition
		if (mEnd != nullptr)
		  next = mEnd->getNext();

 		if (next == nullptr) {
			// at the end of the script
			// returning null means go to OUR next statement, here
			// we need to return the pseudo End statement to make
			// this script terminate
			next = ScriptEndStatement::Pseudo;
		}
	}

    return next;
}

bool ScriptRepeatStatement::isDone(ScriptInterpreter* si)
{
	bool done = false;

	ScriptStack* stack = si->getStack();

	if (stack == nullptr) {
		Trace(1, "Script %s: Repeat lost iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else if (stack->getIterator() != this) {
		// this isn't ours!
		Trace(1, "Script %s: Repeat mismatched iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else {
		done = stack->nextIndex();
		if (done)
		  Trace(3, "Script %s: end of Repeat\n", si->getTraceName());
	}

    return done;
}

/****************************************************************************
 *                                                                          *
 *   								WHILE                                   *
 *                                                                          *
 ****************************************************************************/

ScriptWhileStatement::ScriptWhileStatement(ScriptCompiler* comp, 
                                           char* args)
{
    (void)comp;
	mExpression = comp->parseExpression(this, args);
}

const char* ScriptWhileStatement::getKeyword()
{
    return "While";
}

/**
 * Assume for now that we an only specify a number of repetitions
 * e.g. "While 2" for 2 repeats.  Eventually could have more flexible
 * iteration ranges like "While 4 8" meaning iterate from 4 to 8 by 1, 
 * but I can't see a need for that yet.
 */
ScriptStatement* ScriptWhileStatement::eval(ScriptInterpreter* si)
{
	ScriptStatement* next = nullptr;

	if (mExpression != nullptr && mExpression->evalToBool(si)) {

		// push a block frame to hold iteration state
		// ScriptStack* stack = si->pushStack(this);
		(void)si->pushStack(this);
	}
	else {
		// while condition started off bad, just bad
		// treat this like an If with a false condition
		if (mEnd != nullptr)
		  next = mEnd->getNext();

 		if (next == nullptr) {
			// at the end of the script
			// returning null means go to OUR next statement, here
			// we need to return the pseudo End statement to make
			// this script terminate
			next = ScriptEndStatement::Pseudo;
		}
	}

    return next;
}

bool ScriptWhileStatement::isDone(ScriptInterpreter* si)
{
	bool done = false;

	ScriptStack* stack = si->getStack();

	if (stack == nullptr) {
		Trace(1, "Script %s: While lost iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else if (stack->getIterator() != this) {
		// this isn't ours!
		Trace(1, "Script %s: While mismatched iteration frame!\n",
              si->getTraceName());
		done = true;
	}
	else if (mExpression == nullptr) {
		// shouldn't have bothered by now
		Trace(1, "Script %s: While without conditional expression!\n",
              si->getTraceName());
		done = true;
	}
	else {
		done = !mExpression->evalToBool(si);
		if (done)
		  Trace(3, "Script %s: end of While\n", si->getTraceName());
	}

    return done;
}

/****************************************************************************
 *                                                                          *
 *                                    NEXT                                  *
 *                                                                          *
 ****************************************************************************/

ScriptNextStatement::ScriptNextStatement(ScriptCompiler* comp, 
                                         char* args)
{
    (void)comp;
    (void)args;
    mIterator = nullptr;
}

const char* ScriptNextStatement::getKeyword()
{
    return "Next";
}

bool ScriptNextStatement::isNext()
{
    return true;
}

void ScriptNextStatement::resolve(Mobius* m)
{
    (void)m;
    // locate the nearest For/Repeat statement
    mIterator = mParentBlock->findIterator(this);

	// iterators don't know how to resolve the next, so tell it
	if (mIterator != nullptr)
	  mIterator->setEnd(this);
}

ScriptStatement* ScriptNextStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = nullptr;

	if (mIterator == nullptr) {
		// unmatched next, ignore
	}
	else if (!mIterator->isDone(si)) {
		next = mIterator->getNext();
	}
	else {
		// we should have an iteration frame on the stack, pop it
		ScriptStack* stack = si->getStack();
		if (stack != nullptr && stack->getIterator() == mIterator)
		  si->popStack();
		else {
			// odd, must be a mismatched next?
			Trace(1, "Script %s: Next no iteration frame!\n",
                  si->getTraceName());
		}
	}

    return next;
}

/****************************************************************************
 *                                                                          *
 *   								SETUP                                   *
 *                                                                          *
 ****************************************************************************/

ScriptSetupStatement::ScriptSetupStatement(ScriptCompiler* comp,	
                                           char* args)
{
    (void)comp;
    // This needs to take the entire argument list as a literal string
    // so we can have spaces in the setup name.
    // !! need to trim
    setArg(args, 0);
}

const char* ScriptSetupStatement::getKeyword()
{
    return "Setup";
}

void ScriptSetupStatement::resolve(Mobius* m)
{
	mSetup.resolve(m, mParentBlock, mArgs[0]);
}

ScriptStatement* ScriptSetupStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

	mSetup.get(si, &v);
	const char* name = v.getString();

    Trace(2, "Script %s: Setup %s\n", si->getTraceName(), name);

    Mobius* m = si->getMobius();
    MobiusConfig* config = m->getConfiguration();
	Setup* s = config->getSetup(name);

    // if a name lookup didn't work it may be a number, 
    // these will be zero based!!
    if (s == nullptr)
      s = config->getSetup(ToInt(name));

    if (s != nullptr) {
        // could pass ordinal here too...
		//m->setActiveSetup(s->getName());
        Trace(1, "ScriptSetupStatement: Unable to change setups");
	}

    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                   PRESET                                 *
 *                                                                          *
 ****************************************************************************/

ScriptPresetStatement::ScriptPresetStatement(ScriptCompiler* comp,
                                             char* args)
{
    (void)comp;
    // This needs to take the entire argument list as a literal string
    // so we can have spaces in the preset name.
    // !! need to trim
    setArg(args, 0);
}

const char* ScriptPresetStatement::getKeyword()
{
    return "Preset";
}

void ScriptPresetStatement::resolve(Mobius* m)
{
	mPreset.resolve(m, mParentBlock, mArgs[0]);
}

ScriptStatement* ScriptPresetStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

	mPreset.get(si, &v);
	const char* name = v.getString();
    Trace(2, "Script %s: Preset %s\n", si->getTraceName(), name);

    Mobius* m = si->getMobius();
    MobiusConfig* config = m->getConfiguration();
    Preset* p = config->getPreset(name);

    // if a name lookup didn't work it may be a number, 
    // these will be zero based!
    if (p == nullptr)
      p = config->getPreset(ToInt(name));

    if (p != nullptr) {
		Track* t = si->getTargetTrack();
		if (t == nullptr)
		  t = m->getTrack();

        // this is of course all different now
        // it should be forwarding the whole thing to TrackManager/LogicalTrack
        //t->changePreset(p->ordinal);
        Trace(1, "Script::ScriptPresetStatement Unable to change presets");
    }

    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                              UNIT TEST SETUP                             *
 *                                                                          *
 ****************************************************************************/

// new: originaly this just called a Mobius function synchronously
// but now that we defer sample installation, this has to be a
// KernelEvent to the shell we wait on

ScriptUnitTestSetupStatement::ScriptUnitTestSetupStatement(ScriptCompiler* comp, 
                                                           char* args)
{
    (void)comp;
    (void)args;
}

const char* ScriptUnitTestSetupStatement::getKeyword()
{
    return "UnitTestSetup";
}

ScriptStatement* ScriptUnitTestSetupStatement::eval(ScriptInterpreter* si)
{
    Trace(2, "Script %s: UnitTestSetup\n", si->getTraceName());

    // start with a GlobalReset to make sure the engine
    // is quiet for the UnitTestSetup event handler
    Mobius* m = si->getMobius();
    m->globalReset(nullptr);

    // now push up to the shell for complex configuration
    KernelEvent* e = si->newKernelEvent();
    e->type = EventUnitTestSetup;
    // any args of interest?
    // if we're already in "unit test mode" could disable it if you do it again
    si->sendKernelEvent(e);

    return nullptr;
}

/**
 * An older function, shouldn't be using this any more!
 */
ScriptInitPresetStatement::ScriptInitPresetStatement(ScriptCompiler* comp, 
                                                     char* args)
{
    (void)comp;
    (void)args;
}

const char* ScriptInitPresetStatement::getKeyword()
{
    return "InitPreset";
}

/**
 * !! This doesn't fit with the new model for editing configurations.
 */
ScriptStatement* ScriptInitPresetStatement::eval(ScriptInterpreter* si)
{
    Trace(1, "Script %s: InitPreset\n", si->getTraceName());
#if 0
    // Not sure if this is necessary but we always started with the preset
    // from the active track then set it in the track specified in the
    // ScriptContext, I would expect them to be the same...

    Mobius* m = si->getMobius();
    Track* srcTrack = m->getTrack();

    // this uses an obscure back-door to pull out the local Preset
    // copy from the Track, modify it, then refresh it
    Preset* p = srcTrack->getPreset();
    p->reset();

	// propagate this immediately to the track (avoid a pending preset)
	// so we can start calling set statements  
	Track* destTrack = si->getTargetTrack();
	if (destTrack == nullptr)
	  destTrack = srcTrack;
    else if (destTrack != srcTrack)
      Trace(1, "Script %s: ScriptInitPresetStatement: Unexpected destination track\n",
            si->getTraceName());

    destTrack->refreshPreset(p);
#endif
    
    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                   BREAK                                  *
 *                                                                          *
 ****************************************************************************/
/*
 * This is used to set flags that will enable code paths where debugger
 * breakpoints may have been set.  Loop has it's own internal field
 * that it monitors, we also have a global ScriptBreak that can be used
 * elsewhere.
 */

bool ScriptBreak = false;

ScriptBreakStatement::ScriptBreakStatement(ScriptCompiler* comp, 
                                           char* args)
{
    (void)comp;
    (void)args;
}

const char* ScriptBreakStatement::getKeyword()
{
    return "Break";
}

ScriptStatement* ScriptBreakStatement::eval(ScriptInterpreter* si)
{
    Trace(3, "Script %s: break\n", si->getTraceName());
	ScriptBreak = true;
    Loop* loop = si->getTargetTrack()->getLoop();
    loop->setBreak(true);
    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                    LOAD                                  *
 *                                                                          *
 ****************************************************************************/

ScriptLoadStatement::ScriptLoadStatement(ScriptCompiler* comp,
                                         char* args)
{
    (void)comp;
    parseArgs(args);
}

const char* ScriptLoadStatement::getKeyword()
{
    return "Load";
}

ScriptStatement* ScriptLoadStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

	si->expandFile(mArgs[0], &v);
	const char* file = v.getString();

	Trace(2, "Script %s: load %s\n", si->getTraceName(), file);
    si->sendKernelEvent(EventLoadLoop, file);

    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                    SAVE                                  *
 *                                                                          *
 ****************************************************************************/

ScriptSaveStatement::ScriptSaveStatement(ScriptCompiler* comp,
                                         char* args)
{
    (void)comp;
    parseArgs(args);
}

const char* ScriptSaveStatement::getKeyword()
{
    return "Save";
}

ScriptStatement* ScriptSaveStatement::eval(ScriptInterpreter* si)
{
	ExValue v;

	si->expandFile(mArgs[0], &v);
	const char* file = v.getString();

    Trace(2, "Script %s: save %s\n", si->getTraceName(), file);

    if (strlen(file) > 0) {
        si->sendKernelEvent(EventSaveProject, file);
    }

    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                    DIFF                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Original syntax required an "audio" argument to diff audio.
 * Since that's the usual case we'll make that optional and
 * require "text" to make it do a text diff.  Will have
 * to change the old scripts that use that but they're very few.
 *
 * If "reverse" was the first arg, then this is an audio diff
 * in reverse.
 */
ScriptDiffStatement::ScriptDiffStatement(ScriptCompiler* comp,
                                         char* args)
{
    (void)comp;
	mText = false;
	mReverse = false;
    mFirstArg = 0;
    
    parseArgs(args);

	if (StringEqualNoCase(mArgs[0], "audio")) {
        // backward compatibility, it is now the default and doesn't need to be included
        mFirstArg = 1;
    }
	else if (StringEqualNoCase(mArgs[0], "reverse")) {
		mReverse = true;
        mFirstArg = 1;
	}
	else if (StringEqualNoCase(mArgs[0], "text")) {
		mText = true;
        mFirstArg = 1;
	}
}

const char* ScriptDiffStatement::getKeyword()
{
    return "Diff";
}

/**
 * Most scripts will omit the second file name
 */
ScriptStatement* ScriptDiffStatement::eval(ScriptInterpreter* si)
{
	ExValue file1;
	ExValue file2;

	si->expandFile(mArgs[mFirstArg], &file1);
	si->expandFile(mArgs[mFirstArg + 1], &file2);
    Trace(2, "Script %s: diff %s %s\n", si->getTraceName(), 
          file1.getString(), file2.getString());

	KernelEventType type = (mText) ? EventDiff : EventDiffAudio;
    KernelEvent* e = si->newKernelEvent();
    e->type = type;
    e->setArg(0, file1.getString());
    e->setArg(1, file2.getString());
	if (mReverse)
	  e->setArg(2, "reverse");

    si->sendKernelEvent(e);

    return nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// Warp
//
// This is a temporary kludge for TestDriver until we can rewrite
// the language to support varialbe Calls or some other way to pass
// in execution entry points rather than always going top to bottom.
// What this does is look in actionArgs for a name.  This was copied
// from the Action.bindingArgs used to run the script and for TestDriver
// will be set in code to the name of the test we want to run.
//
// If this is set, it acts like a Call to the Proc with that name.
// After the Proc is finished the entire script ends.  Unlike other
// statements, we don't just resume execution after the Warp statement.
//
//////////////////////////////////////////////////////////////////////

ScriptWarpStatement::ScriptWarpStatement(ScriptCompiler* comp,
                                         char* args)
{
    (void)comp;
    (void)args;
}

ScriptWarpStatement::~ScriptWarpStatement()
{
}

const char* ScriptWarpStatement::getKeyword()
{
    return "Warp";
}

void ScriptWarpStatement::resolve(Mobius* m)
{
    (void)m;
}

void ScriptWarpStatement::link(ScriptCompiler* comp)
{
    (void)comp;
}

ScriptStatement* ScriptWarpStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = nullptr;

    // todo: juse make this use a $ reference now that we have them
    const char* procname = si->getActionArgs();
    if (strlen(procname) == 0) {
        Trace(2, "ScriptWarp: No Proc name specified\n");
    }
    else {
        ScriptProcStatement* proc = mParentBlock->findProc(procname);
        if (proc == nullptr) {
            Trace(1, "ScriptWarp: Unresolved Proc %s\n", procname);
        }
        else {
            Trace(2, "ScriptWarp: Warping to Proc %s\n", procname);
            ScriptBlock* block = proc->getChildBlock();
            if (block != nullptr && block->getStatements() != nullptr) {
                // this is where Call would evaluate the argument
                si->pushStack(this, proc);
                next = block->getStatements();
            }
        }
    }
    
    return next;
}

/****************************************************************************
 *                                                                          *
 *                                    CALL                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Leave the arguments raw and resolve then dynamically at runtime.
 * Could be smarter about this, but most of the time the arguments
 * are used to build file paths and need dynamic expansion.
 */
ScriptCallStatement::ScriptCallStatement(ScriptCompiler* comp,
                                         char* args)
{
	mProc = nullptr;
	mScript = nullptr;
    mExpression = nullptr;

	// isolate the first argument representing the name of the
    // thing to call, the remainder is an expression
	args = parseArgs(args, 0, 1);

    if (args != nullptr)
      mExpression = comp->parseExpression(this, args);
}

ScriptCallStatement::~ScriptCallStatement()
{
    delete mExpression;
}

const char* ScriptCallStatement::getKeyword()
{
    return "Call";
}

/**
 * Start by resolving within the script.
 * If we don't find a proc, then later during link()
 * we'll look for other scripts.
 */
void ScriptCallStatement::resolve(Mobius* m)
{
    (void)m;
	// think locally, then globally
	mProc = mParentBlock->findProc(mArgs[0]);
	
    // TODO: I don't like deferring resolution within
    // the ExNode until the first evaluation.  Find a way to do at least
    // most of them now.
}

/**
 * Resolve a call to another script in the environment.
 */
void ScriptCallStatement::link(ScriptCompiler* comp)
{
	if (mProc == nullptr && mScript == nullptr) {

		mScript = comp->resolveScript(mArgs[0]);
		if (mScript == nullptr)
		  Trace(1, "Script %s: Unresolved call to %s\n", 
				comp->getScript()->getTraceName(), mArgs[0]);
	}
}

ScriptStatement* ScriptCallStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = nullptr;

	if (mProc != nullptr) {    
        ScriptBlock* block = mProc->getChildBlock();
        if (block != nullptr && block->getStatements() != nullptr) {
            // evaluate the argument list
            // !! figure out a way to pool ExNodes with ExValueLists 
            // in ScriptStack 
            
            ExValueList* args = nullptr;
            if (mExpression != nullptr)
              args = mExpression->evalToList(si);

            si->pushStack(this, si->getScript(), mProc, args);
            next = block->getStatements();
        }
	}
	else if (mScript != nullptr) {

        ScriptBlock* block = mScript->getBlock();
        if (block != nullptr && block->getStatements() != nullptr) {

            // !! have to be careful with autoload from another "thread"
            // if we have a call in progress, need a reference count or 
            // something on the Script

            ExValueList* args = nullptr;
            if (mExpression != nullptr)
              args = mExpression->evalToList(si);

            si->pushStack(this, mScript, nullptr, args);
            // and start executing the child script
            next = block->getStatements();
        }
	}
	else {
		Trace(1, "Script %s: Unresolved call: %s\n", si->getTraceName(), mArgs[0]);
	}

    return next;
}

/****************************************************************************
 *                                                                          *
 *                                   START                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * A variant of Call that only does scripts, and launches them
 * in a parallel thread.
 */
ScriptStartStatement::ScriptStartStatement(ScriptCompiler* comp,
                                           char* args)
{
	mScript = nullptr;
    mExpression = nullptr;

	// isolate the first argument representing the name of the
    // thing to call, the remainder is an expression
	args = parseArgs(args, 0, 1);

    if (args != nullptr)
      mExpression = comp->parseExpression(this, args);
}

const char* ScriptStartStatement::getKeyword()
{
    return "Start";
}

/**
 * Find the referenced script.
 */
void ScriptStartStatement::link(ScriptCompiler* comp)
{
	if (mScript == nullptr) {
		mScript = comp->resolveScript(mArgs[0]);
		if (mScript == nullptr)
		  Trace(1, "Script %s: Unresolved call to %s\n", 
				comp->getScript()->getTraceName(), mArgs[0]);
	}
}

ScriptStatement* ScriptStartStatement::eval(ScriptInterpreter* si)
{
    (void)si;
    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                  BLOCKING                                *
 *                                                                          *
 ****************************************************************************/

ScriptBlockingStatement::ScriptBlockingStatement()
{
    mChildBlock = nullptr;
}

ScriptBlockingStatement::~ScriptBlockingStatement()
{
    delete mChildBlock;
}

/**
 * Since we are a blocking statement have to do recursive resolution.
 */
void ScriptBlockingStatement::resolve(Mobius* m)
{
    if (mChildBlock != nullptr)
      mChildBlock->resolve(m);
}

/**
 * Since we are a blocking statement have to do recursive linking.
 */
void ScriptBlockingStatement::link(ScriptCompiler* compiler)
{
    if (mChildBlock != nullptr)
      mChildBlock->link(compiler);
}

ScriptBlock* ScriptBlockingStatement::getChildBlock()
{
    if (mChildBlock == nullptr)
      mChildBlock = NEW(ScriptBlock);
    return mChildBlock;
}

/****************************************************************************
 *                                                                          *
 *   								 PROC                                   *
 *                                                                          *
 ****************************************************************************/

ScriptProcStatement::ScriptProcStatement(ScriptCompiler* comp, 
                                         char* args)
{
    (void)comp;
    parseArgs(args);
}

const char* ScriptProcStatement::getKeyword()
{
    return "Proc";
}

bool ScriptProcStatement::isProc()
{
    return true;
}

const char* ScriptProcStatement::getName()
{
	return getArg(0);
}

ScriptStatement* ScriptProcStatement::eval(ScriptInterpreter* si)
{
    (void)si;
	// no side effects, wait for a call
    return nullptr;
}

//
// Endproc
//

ScriptEndprocStatement::ScriptEndprocStatement(ScriptCompiler* comp,
                                               char* args)
{
    (void)comp;
    parseArgs(args);
}

const char* ScriptEndprocStatement::getKeyword()
{
    return "Endproc";
}

bool ScriptEndprocStatement::isEndproc()
{
	return true;
}

/**
 * No side effects, in fact we normally won't even keep these
 * in the compiled script now that Proc statements are nested.
 */
ScriptStatement* ScriptEndprocStatement::eval(ScriptInterpreter* si)
{
    (void)si;
	return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                                 PARAMETER                                *
 *                                                                          *
 ****************************************************************************/

ScriptParamStatement::ScriptParamStatement(ScriptCompiler* comp, 
                                           char* args)
{
    (void)comp;
    parseArgs(args);
}

const char* ScriptParamStatement::getKeyword()
{
    return "Param";
}

bool ScriptParamStatement::isParam()
{
    return true;
}

const char* ScriptParamStatement::getName()
{
	return getArg(0);
}

/**
 * Scripts cannot "call" these, the statements will be found
 * by Mobius automatically when scripts are loaded and converted
 * into Parameters.
 */
ScriptStatement* ScriptParamStatement::eval(ScriptInterpreter* si)
{
    (void)si;
	// no side effects, wait for a reference
    return nullptr;
}

//
// Endparam
//

ScriptEndparamStatement::ScriptEndparamStatement(ScriptCompiler* comp,
                                                 char* args)
{
    (void)comp;
    parseArgs(args);
}

const char* ScriptEndparamStatement::getKeyword()
{
    return "Endparam";
}

bool ScriptEndparamStatement::isEndparam()
{
	return true;
}

/**
 * No side effects, in fact we normally won't even keep these
 * in the compiled script.
 */
ScriptStatement* ScriptEndparamStatement::eval(ScriptInterpreter* si)
{
    (void)si;
	return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                             FUNCTION STATEMENT                           *
 *                                                                          *
 ****************************************************************************/

/**
 * We assume arguments are expressions unless we can resolve to a static
 * function and it asks for old-school arguments.
 */
ScriptFunctionStatement::ScriptFunctionStatement(ScriptCompiler* comp,
                                                 const char* name,
                                                 char* args)
{
	init();

	mFunctionName = CopyString(name);

	// This is kind of a sucky reserved argument convention...
    // new: honestly, this sort of tokenizing is better than what parseArgs
    // does allocating new strings for each token
	char* ptr = comp->skipToken(args, "up");
    if (ptr != nullptr) {
        mUp = true;
        args = ptr;
    }
    else {
        ptr = comp->skipToken(args, "down");
        if (ptr != nullptr) {
            // it isn't enough just to use !mUp, there is logic below that
            // needs to know if an explicit up/down argument was passed
            mDown = true;
            args = ptr;
        }
    }

    // Resolve the Function
    // Note that we only look at static BehaviorFunction symbols here,
    // Cross-script references that use RunScriptFunction are resolved
    // in link() below.  While the SymbolTable may have BehaviorScript
    // symbols with a matching name, those are for the PREVIOUS script
    // complication and are about to be replaced once this complication finishes
    SymbolTable* symbols = comp->getMobius()->getContainer()->getSymbols();
    for (auto symbol : symbols->getSymbols()) {
        if (symbol->coreFunction != nullptr) {
            Function* f = (Function*)symbol->coreFunction;
            // note we use isMatch here to support aliases and display
            // names, not sure if this is still necessary
            if (f != nullptr && f->isMatch(mFunctionName)) {
                mFunction = f;
                break;
            }
        }
    }

    if (mFunction != nullptr && 
        (!mFunction->expressionArgs && !mFunction->variableArgs)) {
        // old way
        parseArgs(args);
    }
    else {
        // parse the whole thing as an expression which may result
        // in a list
        mExpression = comp->parseExpression(this, args);
    }
}

/**
 * This is only used when script recording is enabled.
 */
ScriptFunctionStatement::ScriptFunctionStatement(Function* f)
{
	init();
	mFunctionName = CopyString(f->getName());
    mFunction = f;
}

void ScriptFunctionStatement::init()
{
    // the four ScriptArguments initialize themselves
	mFunctionName = nullptr;
	mFunction = nullptr;
	mUp = false;
    mDown = false;
    mExpression = nullptr;
}

ScriptFunctionStatement::~ScriptFunctionStatement()
{
	delete mFunctionName;
	delete mExpression;
    // note that we do not own mFunction, it is usually static,
    // if not it is owned by the Script
}

/**
 * If we have a static function, resolve the arguments if
 * the function doesn't support expressions.
 */
void ScriptFunctionStatement::resolve(Mobius* m)
{
    if (mFunction != nullptr && 
        // if we resolved this to a script always use expressions
        // !! just change RunScriptFunction to set expressionArgs?
        mFunction->eventType != RunScriptEvent && 
        !mFunction->expressionArgs &&
        !mFunction->variableArgs) {

        mArg1.resolve(m, mParentBlock, mArgs[0]);
        mArg2.resolve(m, mParentBlock, mArgs[1]);
        mArg3.resolve(m, mParentBlock, mArgs[2]);
        mArg4.resolve(m, mParentBlock, mArgs[3]);
    }
}

/**
 * Resolve function-style references to other scripts.
 *
 * We allow function statements whose keywoards are the names of scripts
 * rather than being prefixed by the "Call" statement.  This makes
 * them behave like more like normal functions with regards to quantization
 * and focus lock.  When we find those references, we bootstrap a set
 * of RunScriptFunction objects to represent the script in the function table.
 * Eventually these will be installed in the global function table.
 *
 * Arguments have already been parsed.
 */
void ScriptFunctionStatement::link(ScriptCompiler* comp)
{
	if (mFunction == nullptr) {

        Script* callingScript = comp->getScript();

        if (mFunctionName == nullptr) {
            Trace(1, "Script %s: missing function name\n", 
                  callingScript->getTraceName());
            Trace(1, "--> File %s line %ld\n",
                  callingScript->getFilename(), (long)mLineNumber);
        }
        else {
            // look for a script
            Script* calledScript = comp->resolveScript(mFunctionName);
            if (calledScript == nullptr) {
                Trace(1, "Script %s: unresolved script function %s\n", 
                      callingScript->getTraceName(), mFunctionName);
                Trace(1, "--> File %s line %ld\n",
                      callingScript->getFilename(), (long)mLineNumber);
            }
            else {
                Function* rsf = calledScript->getFunction();
                if (rsf == nullptr) {
                    Trace(1, "Script %s: Calling script without a RunScriptFunction\n");
                }
                else {
                    mFunction = rsf;
                }
            }
        }
    }
}

const char* ScriptFunctionStatement::getKeyword()
{
    return mFunctionName;
}

Function* ScriptFunctionStatement::getFunction()
{
	return mFunction;
}

const char* ScriptFunctionStatement::getFunctionName()
{
	return mFunctionName;
}

void ScriptFunctionStatement::setUp(bool b)
{
	mUp = b;
}

bool ScriptFunctionStatement::isUp()
{
	return mUp;
}

ScriptStatement* ScriptFunctionStatement::eval(ScriptInterpreter* si)
{
    // has to be resolved by now...before 2.0 did another search of the
    // Functions table but that shouldn't be necessary??
    Function* func = mFunction;

    if (func  == nullptr) {
        Trace(1, "Script %s: unresolved function %s\n", 
              si->getTraceName(), mFunctionName);
    }
    else {
        Trace(3, "Script %s: %s\n", si->getTraceName(), func->getName());

        Mobius* m = si->getMobius();
        Action* a = m->newAction();

        // target

        a->setFunction(func);
        Track* t = si->getTargetTrack();
        if (t != nullptr) {
            // force it into this track
            a->setResolvedTrack(t);
        }
        else {
            // something is wrong, must have a track! 
            // to make sure focus lock or groups won't be applied
            // set this special flag
            Trace(1, "Script %s: function invoked with no target track %s\n", 
                  si->getTraceName(), mFunctionName);
            a->noGroup = true;
        }

        // trigger

        a->trigger = TriggerScript;

        // this is for GlobalReset handling
        a->triggerOwner = si;

        // would be nice if this were just part of the Function's
        // arglist parsing?
        a->down = !mUp;

		// if there is an explicit "down" argument, assume
		// this is sustainable and there will eventually be the
		// same function with an "up" argument
        if (mUp || mDown)
          a->triggerMode = TriggerModeMomentary;
        else
          a->triggerMode = TriggerModeOnce;
		
		// Note that we are not setting a function trigger here, which
		// at the moment are only used to implement SUS scripts.  Creating
		// a unique id here may be difficult, it could be the Script address
		// but we're not guaranteed to evaluate the up transition in 
		// the same script.

        // once we start using Wait, schedule at absolute times
        a->noLatency = si->isPostLatency();

        // arguments

        if (mExpression == nullptr) {
            // old school single argument
            // do full expansion on these, nice when building path names
            // for SaveFile and SaveRecordedAudio, overkill for everything else
            if (mArg1.isResolved())
              mArg1.get(si, &(a->arg));
            else
              si->expand(mArg1.getLiteral(), &(a->arg));
        }
        else {
            // Complex args, the entire line was parsed as an expression
            // may result in an ExValueList if there were spaces or commas.
            ExValue* value = &(a->arg);
            mExpression->eval(si, value);

            if (func->variableArgs) {
                // normalize to an ExValueList
                if (value->getType() == EX_LIST) {
                    // transfer the value here
                    a->scriptArgs = value->takeList();
                }
                else if (!value->isNull()) {
                    // unusual, promote to a list 
                    ExValue* copy = NEW(ExValue);
                    copy->set(value);
                    ExValueList* list = NEW(ExValueList);
                    list->add(copy);
                    a->scriptArgs = list;
                }
                // in all cases we don't want to leave anything here
                value->setNull();
            }
            else if (value->getType() == EX_LIST) {
                // Multiple values for a function that was only
                // expecting one.  Take the first one and ignore the others
                ExValueList* list = value->takeList();
                if (list != nullptr && list->size() > 0) {
                    ExValue* first = list->getValue(0);
                    // Better not be a nested list here, ugly ownership issues
                    // could handle it but unnecessary
                    if (first->getType() == EX_LIST) 
                      Trace(1, "Script %s: Nested list in script argument!\n",
                            si->getTraceName());
                    else
                      value->set(first);
                }
                delete list;
            }
            else {
                // single value, just leave it in scriptArg
            }
        }

        // make it go!
        m->doOldAction(a);

        si->setLastEvents(a);

        // we always must be notified what happens to this, even
        // if we aren't waiting on it
		// ?? why?  if the script ends without waiting, then we have to 
		// remember to clean up this reference before deleting/pooling
		// the interpreter, I guess that's a good idea anyway
        if (a->getEvent() != nullptr) {
			// TODO: need an argument like "async" to turn off
			// the automatic completion wait, probably only for unit tests.
			if (func->scriptSync)
			  si->setupWaitLast(this);
		}
		else {
			// it happened immediately
			// Kludge: Need to detect changes to the selected track and change
			// what we think the default track is.  No good way to encapsulate
			// this so look for specific function families.

			if (func->eventType == TrackEvent ||	
				func == GlobalReset) {
				// one of the track select functions, change the default track
				si->setTrack(m->getTrack());
			}
		}

        // if the event didn't take it, we can delete it
        m->completeAction(a);
    }
    return nullptr;
}

/****************************************************************************
 *                                                                          *
 *                               WAIT STATEMENT                             *
 *                                                                          *
 ****************************************************************************/

ScriptWaitStatement::ScriptWaitStatement(WaitType type,
                                         WaitUnit unit,
                                         long time)
{
	init();
	mWaitType = type;
	mUnit = unit;
    mExpression = NEW1(ExLiteral, (int)time);
}

void ScriptWaitStatement::init()
{
	mWaitType = WAIT_NONE;
	mUnit = UNIT_NONE;
    mExpression = nullptr;
	mInPause = false;
}

ScriptWaitStatement::~ScriptWaitStatement()
{
	delete mExpression;
}

const char* ScriptWaitStatement::getKeyword()
{
    return "Wait";
}


/**
 * This one is awkward because of the optional keywords.
 *
 * The "time" unit is optional because it is the most common wait,
 * these lines are the same:
 *
 *     Wait time frame 100
 *     Wait frame 100
 *
 * We have even supported optional "frame" unit, this is used in many
 * of the tests:
 *
 *     Wait 100
 *
 * 
 * We used to allow the "function" keyword to be optional but
 * I dont' like that:
 *
 *     Wait function Record
 *     Wait Record
 *
 * Since this was never used I'm going to start requiring it.
 * It is messy to support if the wait time value can be an expression.
 *
 * If that weren't enough, there is an optional "inPause" argument that
 * says that the wait is allowed to proceed during Pause mode.  This
 * is only used in a few tests.  It used to be at the end but was moved
 * to the front when we started allowing value expressions.
 *
 *     Wait inPause frame 1000
 *
 * new: parseArgs is a mess, if you call it more than once it can leak
 * previously parsed args.  Added clearArgs() to explicitly delete
 * prior parse results, would rather this be something parseArgs does
 * every time.
 */
ScriptWaitStatement::ScriptWaitStatement(ScriptCompiler* comp,
                                         char* args)
{
	init();
    
    // this one is odd because of the optional args, parse one at a time
    char* prev = args;
    char* psn = parseArgs(args, 0, 1);

    // consume optional keywords
    if (StringEqualNoCase(mArgs[0], "inPause")) {
        mInPause = true;
        prev = psn;
        // tracking down memory leaks
        clearArgs();
        psn = parseArgs(psn, 0, 1);
    }

    mWaitType = getWaitType(mArgs[0]);

	if (mWaitType == WAIT_NONE) {
        // may be a relative time wait with missing "time"
		mUnit = getWaitUnit(mArgs[0]);
        if (mUnit != UNIT_NONE) {
            // left off the type, assume "time"
            mWaitType = WAIT_RELATIVE;
        }
        else {
            // assume it's "Wait X" 
            // could sniff test the argument?  This is going to make
            // it harder to find invalid statements...
            //Trace(1, "ERROR: Invalid Wait: %s\n", args);
            mWaitType = WAIT_RELATIVE;
            mUnit = UNIT_FRAME;
            // have to rewind since the previous token was part of the expr
            psn = prev;
        }
	}

    if (mWaitType == WAIT_RELATIVE || mWaitType == WAIT_ABSOLUTE) {

        // if mUnit is none, we had the explicit "time" or "until"
        // keyword, parse the unit now
        if (mUnit == UNIT_NONE) {
            prev = psn;
            clearArgs();
            psn = parseArgs(psn, 0, 1);
            mUnit = getWaitUnit(mArgs[0]);
        }

        if (mUnit == UNIT_NONE) {
            // Allow missing unit for "Wait until"
            if (mWaitType != WAIT_ABSOLUTE)
              comp->syntaxError(this, "Invalid Wait");
            else {
                mUnit = UNIT_FRAME;
                psn = prev;
            }
        }

        if (mUnit != UNIT_NONE) {
            // whatever remains is the value expression
            mExpression = comp->parseExpression(this, psn);
        }
    }
	else if (mWaitType == WAIT_FUNCTION) {
        // next arg has the function name, leave in mArgs[0]
        clearArgs();
        parseArgs(psn, 0, 1);
	}
}

WaitType ScriptWaitStatement::getWaitType(const char* name)
{
	WaitType type = WAIT_NONE;
	for (int i = 0 ; WaitTypeNames[i] != nullptr ; i++) {
		if (StringEqualNoCase(WaitTypeNames[i], name)) {
			type = (WaitType)i;
			break;
		}
	}
	return type;
}

WaitUnit ScriptWaitStatement::getWaitUnit(const char* name)
{
	WaitUnit unit = UNIT_NONE;

    // hack, it is common to put an "s" on the end such
    // as "Wait frames 1000" rather than "Wait frame 1000".
    // Since the error isn't obvious catch it here.
    if (name != nullptr) {
        int last = (int)strlen(name) - 1;
        if (last > 0 && name[last] == 's') {
            char buffer[128];
            strcpy(buffer, name);
            buffer[last] = '\0';
            name = buffer;
        }
    }

    for (int i = 0 ; WaitUnitNames[i] != nullptr ; i++) {
        // KLUDGE: recognize old-style plural names for
        // backward compatibility by using StartWith rather than Compare
        // !! this didn't seem to work, had to do the hack above, why?
        if (StartsWithNoCase(name, WaitUnitNames[i])) {
            unit = (WaitUnit)i;
            break;
        }
    }

	return unit;
}

ScriptStatement* ScriptWaitStatement::eval(ScriptInterpreter* si)
{
    // reset the "interrupted" variable
    // will this work without a declaration?
    UserVariables* vars = si->getVariables();
    if (vars != nullptr) {
        ExValue v;
        v.setNull();
        vars->set("interrupted", &v);
    }
    
    switch (mWaitType) {
        case WAIT_NONE: {	
            // probably an error somewhere
            Trace(1,  "Script %s: Malformed script wait statmenet\n",
                  si->getTraceName());
        }
            break;
        case WAIT_LAST: {
            Trace(2,  "Script %s: Wait last\n", si->getTraceName());
            si->setupWaitLast(this);
        }
            break;
        case WAIT_THREAD: {
            Trace(2,  "Script %s: Wait thread\n", si->getTraceName());
            si->setupWaitThread(this);
        }
            break;
        case WAIT_FUNCTION: {
            // !! not sure if this actually works anymore, it was never used...
            // don't have the static Function array any more so have to use SymbolTable
            // this will only find static Functions you can't wait on RunScriptFunction
            // todo: it would be more reliable for anything that resolves through a Symbol
            // to just remember the Symbol since it can become unresolved 
            const char* name = mArgs[0];
            Function* f = nullptr;
            SymbolTable* symbols = si->getMobius()->getContainer()->getSymbols();
            for (auto symbol : symbols->getSymbols()) {
                if (symbol->coreFunction != nullptr &&
                    StringEqual(name, symbol->getName())) {
                    f = (Function*)symbol->coreFunction;
                    break;
                }
            }
            if (f == nullptr)
			  Trace(1, "Script %s: unresolved wait function %s!\n", 
                    si->getTraceName(), name);
			else {
				Trace(2,  "Script %s: Wait function %s\n", si->getTraceName(), name);
				ScriptStack* frame = si->pushStackWait(this);
				frame->setWaitFunction(f);
			}
        }
            break;
		case WAIT_EVENT: {
			// wait for a specific event
            Trace(1,  "Script %s: Wait event not implemented\n", si->getTraceName());
		}
            break;
        case WAIT_UP: {
            Trace(1,  "Script %s: Wait up not implemented\n", si->getTraceName());
        }
            break;
        case WAIT_LONG: {
            Trace(1,  "Script %s: Wait long not implemented\n", si->getTraceName());
        }
            break;
        case WAIT_BLOCK: {
            // wait for the start of the next interrupt
            Trace(3, "Script %s: waiting for next block\n", si->getTraceName());
			ScriptStack* frame = si->pushStackWait(this);
			frame->setWaitBlock(true);
        }
            break;
        case WAIT_SWITCH: {
            // no longer have the "fundamenatal command" concept
            // !! what is this doing?
            Trace(1, "Script %s: wait switch\n", si->getTraceName());
			ScriptStack* frame = si->pushStackWait(this);
			frame->setWaitFunction(Loop1);
        }
            break;
        case WAIT_SCRIPT: {
            // wait for any KernelEvents we've sent to complete
            // !! we don't need this any more now that we have "Wait thread"
            KernelEvent* e = si->newKernelEvent();
            e->type = EventWait;
			ScriptStack* frame = si->pushStackWait(this);
			frame->setWaitKernelEvent(e);
			si->sendKernelEvent(e);
			Trace(3, "Script %s: wait script event\n", si->getTraceName());
		}
            break;

		case WAIT_START:
		case WAIT_END:
		case WAIT_EXTERNAL_START:
		case WAIT_DRIFT_CHECK:
		case WAIT_PULSE:
		case WAIT_BEAT:
		case WAIT_BAR:
		case WAIT_REALIGN:
		case WAIT_RETURN: {

			// Various pending events that wait for Loop or 
			// Synchronizer to active them at the right time.
            // !! TODO: Would be nice to wait for a specific pulse

            Trace(2, "Script %s: wait %s\n", 
                  si->getTraceName(), WaitTypeNames[mWaitType]);
			Event* e = setupWaitEvent(si, 0);
			e->pending = true;
			e->fields.script.waitType = mWaitType;
		}
            break;

        default: {
            // relative, absolute, and audio
			Event* e = setupWaitEvent(si, getWaitFrame(si));
			e->fields.script.waitType = mWaitType;

			// special option to bring us out of pause mode
			// Should really only allow this for absolute millisecond waits?
			// If we're waiting on a cycle should wait for the loop to be
			// recorded and/or leave pause.  Still it could be useful
			// to wait for a loop-relative time.
			e->pauseEnabled = mInPause;

            // !! every relative UNIT_MSEC wait should be implicitly
            // enabled in pause mode.  No reason not to and it's what
            // people expect.  No one will remember "inPause"
            if (mWaitType == WAIT_RELATIVE && mUnit == UNIT_MSEC)
              e->pauseEnabled = true;

            Trace(2, "Script %s: Wait\n", si->getTraceName());
        }
            break;
    }

    // set this to prevent the addition of input latency
    // when scheduling future functions from the script
    si->setPostLatency(true);

    return nullptr;
}

/**
 * Setup a Script event on a specific frame.
 */
Event* ScriptWaitStatement::setupWaitEvent(ScriptInterpreter* si, 
                                           long frame)
{
    Track* track = si->getTargetTrack();
    EventManager* em = track->getEventManager();
	Event* e = em->newEvent();

	e->type = ScriptEvent;
	e->frame = frame;
	e->setScriptInterpreter(si);
	Trace(3, "Script %s: wait for frame %ld\n", si->getTraceName(), e->frame);
	em->addEvent(e);

	ScriptStack* stack = si->pushStackWait(this);
	stack->setWaitEvent(e);

	return e;
}

/**
 * Return the number of frames represented by a millisecond.
 * Adjusted for the current playback rate.  
 * For accurate waits, you have to ensure that the rate can't
 * change while we're waiting.
 */
long ScriptWaitStatement::getMsecFrames(ScriptInterpreter* si, long msecs)
{
	float rate = si->getTargetTrack()->getEffectiveSpeed();
	// should we ceil()?
	long frames = (long)(MSEC_TO_FRAMES(msecs) * rate);
	return frames;
}

/**
 * Calculate the frame at which to schedule a ScriptEvent event after 
 * the desired wait.
 *
 * If we're in the initial record, only WAIT_AUDIO or WAIT_ABSOLULTE with 
 * UNIT_MSEC and UNIT_FRAME are meaningful.  Since it will be a common 
 * error, also recognize WAIT_RELATIVE with UNIT_MSEC and UNIT_FRAME. 
 * If any other unit is specified assume 1 second.
 */
long ScriptWaitStatement::getWaitFrame(ScriptInterpreter* si)
{
	long frame = 0;
    Track* track = si->getTargetTrack();
	Loop* loop = track->getLoop();
	WaitType type = mWaitType;
	WaitUnit unit = mUnit;
	long current = loop->getFrame();
	long loopFrames = loop->getFrames();
	long time = getTime(si);

	if (loopFrames == 0) {
		// initial record
		if (type == WAIT_RELATIVE || type == WAIT_ABSOLUTE) {
			if (unit != UNIT_MSEC && unit != UNIT_FRAME) {
                // !! why have we done this?
                Trace(1, "Script %s: ERROR: Fixing malformed wait during initial record\n",
                      si->getTraceName());
				unit = UNIT_MSEC;
				time = 1000;
			}
		}
	}

	switch (type) {
		
		case WAIT_RELATIVE: {
			// wait some number of frames after the current frame	
			switch (unit) {
				case UNIT_MSEC: {
					frame = current + getMsecFrames(si, time);
				}
                    break;

				case UNIT_FRAME: {
					frame = current + time;
				}
                    break;

				case UNIT_SUBCYCLE: {
					// wait for the start of a subcycle after the current frame
					frame = getQuantizedFrame(loop, 
											  QUANTIZE_SUBCYCLE, 
											  current, time);
				}
                    break;

				case UNIT_CYCLE: {
					// wait for the start of a cycle after the current frame
					frame = getQuantizedFrame(loop, QUANTIZE_CYCLE, 
											  current, time);
				}
                    break;

				case UNIT_LOOP: {
					// wait for the start of a loop after the current frame
					frame = getQuantizedFrame(loop, QUANTIZE_LOOP, 
											  current, time);
				}
                    break;
			
				case UNIT_NONE: break;
			}
		}
            break;

		case WAIT_ABSOLUTE: {
			// wait for a particular frame within the loop
			switch (unit) {
				case UNIT_MSEC: {
					frame = getMsecFrames(si, time);
				}
                    break;

				case UNIT_FRAME: {
					frame = time;
				}
                    break;

				case UNIT_SUBCYCLE: {
					// Hmm, should the subcycle be relative to the
					// start of the loop or relative to the current cycle?
					// Start of the loop feels more natural.
					// If there aren't this many subcycles in a cycle, do 
					// we spill over into the next cycle or round?
					// Spill.
					frame = loop->getSubCycleFrames() * time;
				}
                    break;

				case UNIT_CYCLE: {
					frame = loop->getCycleFrames() * time;
				}
                    break;

				case UNIT_LOOP: {
					// wait for the start of a particular loop
					// this is meaningless since there is only one loop, though
					// I supposed we could take this to mean whenever the
					// loop is triggered, that would be inconsistent with the
					// other absolute time values though.
					// Let this mean to wait for n iterations of the loop
					frame = loop->getFrames() * time;
				}
                    break;

				case UNIT_NONE: break;
			}
		}
            break;

		default:
			// need this or xcode 5 whines
			break;
	}

	return frame;
}

/**
 * Evaluate the time expression and return the result as a long.
 */
long ScriptWaitStatement::getTime(ScriptInterpreter* si)
{
    long time = 0;
    if (mExpression != nullptr) {
		ExValue v;
		mExpression->eval(si, &v);
        time = v.getLong();
    }
    return time;
}

/**
 * Helper for getWaitFrame.
 * Calculate a quantization boundary frame.
 * If we're finishing recording of the initial loop, 
 * don't quantize to the end of the loop, go to the next.
 */
long ScriptWaitStatement::getQuantizedFrame(Loop* loop,
                                            QuantizeMode q, 
                                            long frame, long count)
{
	long loopFrames = loop->getFrames();

	// special case for the initial record, can only get here after
	// we've set the loop frames, but before receiving all of them
	if (loop->getMode() == RecordMode)
	  frame = loopFrames;

	// if count is unspecified it defaults to 1, for the next whatever
	if (count == 0) 
	  count = 1;

    EventManager* em = loop->getTrack()->getEventManager();

	for (int i = 0 ; i < count ; i++) {
		// if we're on a boundary the first time use it, otherwise advance?
		// no, always advance
		//bool after = (i > 0);
		frame = em->getQuantizedFrame(loop, frame, q, true);
	}

	return frame;
}

/****************************************************************************
 *                                                                          *
 *   								SCRIPT                                  *
 *                                                                          *
 ****************************************************************************/

// under what circumstances would we make one of these raw?
// awkward with the RunScriptFunction forced allocation
// just make this a static member

Script::Script()
{
    Trace(1, "Script::Script Why am I here?\n");
	init();
    mFunction = NEW1(RunScriptFunction, this);
}

Script::Script(MScriptLibrary* env, const char* filename)
{
	init();
	mLibrary = env;
    setFilename(filename);
    mFunction = NEW1(RunScriptFunction, this);
}

void Script::init()
{
	mLibrary = nullptr;
	mNext = nullptr;
    mFunction = nullptr;
    mName = nullptr;
	mDisplayName = nullptr;
	mFilename = nullptr;
    mDirectory = nullptr;

	mAutoLoad = false;
	mButton = false;
    mTest = false;
	mFocusLockAllowed = false;
	mQuantize = false;
	mSwitchQuantize = false;
    mExpression = false;
	mContinuous = false;
    mParameter = false;
	mSpread = false;
    mHide = false;
	mSpreadRange = 0;
	mSustainMsecs = DEFAULT_SUSTAIN_MSECS;
	mClickMsecs = DEFAULT_CLICK_MSECS;

    mBlock = nullptr;

	mReentryLabel = nullptr;
	mSustainLabel = nullptr;
	mEndSustainLabel = nullptr;
	mClickLabel = nullptr;	
    mEndClickLabel = nullptr;
}

Script::~Script()
{
	Script *el, *next;

	clear();
    delete mFunction;
	delete mName;
	delete mDisplayName;
	delete mFilename;
	delete mDirectory;

	for (el = mNext ; el != nullptr ; el = next) {
		next = el->getNext();
		el->setNext(nullptr);
		delete el;
	}
}

void Script::setLibrary(MScriptLibrary* env)
{
    mLibrary = env;
}

MScriptLibrary* Script::getLibrary()
{
    return mLibrary;
}

void Script::setNext(Script* s)
{
	mNext = s;
}

Script* Script::getNext()
{
	return mNext;
}

void Script::setName(const char* name)
{
    delete mName;
    mName = CopyString(name);
}

const char* Script::getName()
{
	return mName;
}

const char* Script::getDisplayName()
{
	const char* name = mName;
	if (name == nullptr) {
		if (mDisplayName == nullptr) {
            if (mFilename != nullptr) {
                // derive a display name from the file path
                char dname[1024];
                GetLeafName(mFilename, dname, false);
                mDisplayName = CopyString(dname);
            }
            else {
                // odd, must be an anonymous memory script?
                name = "???";
            }
		}
		name = mDisplayName;
	}
    return name;
}

const char* Script::getTraceName()
{
    // better to always return the file name?
    return getDisplayName();
}

void Script::setFilename(const char* s)
{
    delete mFilename;
    mFilename = CopyString(s);
}

const char* Script::getFilename()
{
    return mFilename;
}

void Script::setDirectory(const char* s)
{
    delete mDirectory;
    mDirectory = CopyString(s);
}

void Script::setDirectoryNoCopy(char* s)
{
    delete mDirectory;
    mDirectory = s;
}

const char* Script::getDirectory()
{
    return mDirectory;
}

//
// Statements
//

void Script::clear()
{
    delete mBlock;
    mBlock = nullptr;
	mReentryLabel = nullptr;
	mSustainLabel = nullptr;
	mEndSustainLabel = nullptr;
	mClickLabel = nullptr;
	mEndClickLabel = nullptr;
}

ScriptBlock* Script::getBlock()
{
    if (mBlock == nullptr)
      mBlock = NEW(ScriptBlock);
    return mBlock;
}

//
// parsed options
//

void Script::setAutoLoad(bool b)
{
	mAutoLoad = b;
}

bool Script::isAutoLoad()
{
	return mAutoLoad;
}

void Script::setButton(bool b)
{
	mButton = b;
}

bool Script::isButton()
{
	return mButton;
}

void Script::setTest(bool b)
{
	mTest = b;
}

bool Script::isTest()
{
	return mTest;
}

void Script::setHide(bool b)
{
	mHide = b;
}

bool Script::isHide()
{
	return mHide;
}

void Script::setFocusLockAllowed(bool b)
{
	mFocusLockAllowed = b;
}

bool Script::isFocusLockAllowed()
{
	return mFocusLockAllowed;
}

void Script::setQuantize(bool b)
{
	mQuantize = b;
}

bool Script::isQuantize()
{
	return mQuantize;
}

void Script::setSwitchQuantize(bool b)
{
	mSwitchQuantize = b;
}

bool Script::isSwitchQuantize()
{
	return mSwitchQuantize;
}

void Script::setContinuous(bool b)
{
	mContinuous = b;
}

bool Script::isContinuous()
{
	return mContinuous;
}

void Script::setParameter(bool b)
{
	mParameter = b;
}

bool Script::isParameter()
{
	return mParameter;
}

void Script::setSpread(bool b)
{
	mSpread = b;
}

bool Script::isSpread()
{
	return mSpread;
}

void Script::setSpreadRange(int i)
{
	mSpreadRange = i;
}

int Script::getSpreadRange()
{
	return mSpreadRange;
}

void Script::setSustainMsecs(int msecs)
{
	if (msecs > 0)
	  mSustainMsecs = msecs;
}

int Script::getSustainMsecs()
{
	return mSustainMsecs;
}

void Script::setClickMsecs(int msecs)
{
	if (msecs > 0)
	  mClickMsecs = msecs;
}

int Script::getClickMsecs()
{
	return mClickMsecs;
}

//
// Cached labels
//

void Script::cacheLabels()
{
    if (mBlock != nullptr) {
        for (ScriptStatement* s = mBlock->getStatements() ; 
             s != nullptr ; s = s->getNext()) {

            if (s->isLabel()) {
                ScriptLabelStatement* l = (ScriptLabelStatement*)s;
                if (l->isLabel(LABEL_REENTRY))
                  mReentryLabel = l;
                else if (l->isLabel(LABEL_SUSTAIN))
                  mSustainLabel = l;
                else if (l->isLabel(LABEL_END_SUSTAIN))
                  mEndSustainLabel = l;
                else if (l->isLabel(LABEL_CLICK))
                  mClickLabel = l;
                else if (l->isLabel(LABEL_END_CLICK))
                  mEndClickLabel = l;
            }
        }
    }
}

ScriptLabelStatement* Script::getReentryLabel()
{
	return mReentryLabel;
}

ScriptLabelStatement* Script::getSustainLabel()
{
	return mSustainLabel;
}

ScriptLabelStatement* Script::getEndSustainLabel()
{
	return mEndSustainLabel;
}

bool Script::isSustainAllowed()
{
	return (mSustainLabel != nullptr || mEndSustainLabel != nullptr);
}

ScriptLabelStatement* Script::getClickLabel()
{
	return mClickLabel;
}

ScriptLabelStatement* Script::getEndClickLabel()
{
	return mEndClickLabel;
}

bool Script::isClickAllowed()
{
	return (mClickLabel != nullptr || mEndClickLabel != nullptr);
}

void Script::setFunction(RunScriptFunction* f)
{
    (void)f;
    Trace(1, "Script::setFunction Not supposed to be calling this\n");
    // mFunction = f;
}

RunScriptFunction* Script::getFunction()
{
    return mFunction;
}

//////////////////////////////////////////////////////////////////////
//
// Compilation
//
//////////////////////////////////////////////////////////////////////

/**
 * Resolve references in a script after it has been fully parsed.
 */
void Script::resolve(Mobius* m)
{
    if (mBlock != nullptr)
      mBlock->resolve(m);
    
    // good place to do this too
    cacheLabels();
}

/**
 * Resolve references between scripts after the entire environment
 * has been loaded.  This will do nothing except for ScriptCallStatement
 * and ScriptStartStatement which will call back to resolveScript to find
 * the referenced script.  Control flow is a bit convoluted but the
 * alternatives aren't much better.
 */
void Script::link(ScriptCompiler* comp)
{
    if (mBlock != nullptr)
      mBlock->link(comp);
}

/**
 * Can assume this is a full path 
 *
 * !!! This doesn't handle blocking statements, Procs won't write
 * properly.  Where is this used?
 */
#if 0
void Script::xwrite(const char* filename)
{
	FILE* fp = fopen(filename, "w");
	if (fp == nullptr) {
		// need a configurable alerting mechanism
		Trace(1, "Script %s: Unable to open file for writing: %s\n", 
              getDisplayName(), filename);
	}
	else {
        // !! write the options
        if (mBlock != nullptr) {
            for (ScriptStatement* a = mBlock->getStatements() ; 
                 a != nullptr ; a = a->getNext())
              a->xwrite(fp);
            fclose(fp);
        }
	}
}
#endif

//////////////////////////////////////////////////////////////////////
//
// MScriptLibrary
//
//////////////////////////////////////////////////////////////////////

MScriptLibrary::MScriptLibrary()
{
    mNext = nullptr;
    mSource = nullptr;
	mScripts = nullptr;
}

MScriptLibrary::~MScriptLibrary()
{
	MScriptLibrary *el, *next;

    delete mSource;
    delete mScripts;

	for (el = mNext ; el != nullptr ; el = next) {
		next = el->getNext();
		el->setNext(nullptr);
		delete el;
	}
}

MScriptLibrary* MScriptLibrary::getNext()
{
    return mNext;
}

void MScriptLibrary::setNext(MScriptLibrary* env)
{
    mNext = env;
}

ScriptConfig* MScriptLibrary::getSource()
{
    return mSource;
}

void MScriptLibrary::setSource(ScriptConfig* config)
{
    delete mSource;
    mSource = config;
}

Script* MScriptLibrary::getScripts()
{
	return mScripts;
}

void MScriptLibrary::setScripts(Script* scripts)
{
    delete mScripts;
    mScripts = scripts;
}

/**
 * Detect differences after editing the script config.
 * We assume the configs are the same if the same names appear in both lists
 * ignoring order.
 *
 * Since our mScripts list can contain less than what was in the original
 * ScriptConfig due to filtering out invalid names, compare with the
 * original ScriptConfig which we saved at compilation.
 */
bool MScriptLibrary::isDifference(ScriptConfig* config)
{
    bool difference = false;

    if (mSource == nullptr) {
        // started with nothing
        if (config != nullptr && config->getScripts() != nullptr)
          difference = true;
    }
    else {
        // let the configs compare themselves
        difference = mSource->isDifference(config);
    }

    return difference;
}

/**
 * Search for a new version of the given script.
 * This is used to refresh previously resolved ResolvedTarget after
 * the scripts are reloaded.  
 *
 * We search using the same name that was used in the binding,
 * which is the script "display name". This is either the !name
 * if it was specified or the base file name.
 * Might want to search on full path to be safe?
 */
Script* MScriptLibrary::getScript(Script* src)
{
    Script* found = nullptr;

    for (Script* s = mScripts ; s != nullptr ; s = s->getNext()) {
        if (StringEqual(s->getDisplayName(), src->getDisplayName())) {
            found = s;
            break;
        }
    }
    return found;
}


/****************************************************************************
 *                                                                          *
 *                                   STACK                                  *
 *                                                                          *
 ****************************************************************************/

ScriptStack::ScriptStack()
{
    // see comments in init() for why this has to be set nullptr first
    mArguments = nullptr;
	init();
}

/**
 * Called to initialize a stack frame when it is allocated
 * for the first time and when it is removed from the pool.
 * NOTE: Handling of mArguments is special because we own it, 
 * everything else is just a reference we can nullptr.  The constructor
 * must set mArguments to nullptr before calling this.
 */
void ScriptStack::init()
{
    mStack = nullptr;
	mScript = nullptr;
    mCall = nullptr;
    mWarp = nullptr;
	mIterator = nullptr;
    mLabel = nullptr;
    mSaveStatement = nullptr;
	mWait = nullptr;
	mWaitEvent = nullptr;
	mWaitKernelEvent = nullptr;
	mWaitFunction = nullptr;
	mWaitBlock = false;
	mMax = 0;
	mIndex = 0;

	for (int i = 0 ; i < MAX_TRACKS ; i++)
	  mTracks[i] = nullptr;

    // This is the only thing we own
    delete mArguments;
    mArguments = nullptr;
}

ScriptStack::~ScriptStack()
{
    // !! need to be pooling these
    delete mArguments;
}

void ScriptStack::setScript(Script* s)
{
    mScript = s;
}

Script* ScriptStack::getScript()
{
    return mScript;
}

void ScriptStack::setProc(ScriptProcStatement* p)
{
    mProc = p;
}

ScriptProcStatement* ScriptStack::getProc()
{
    return mProc;
}

void ScriptStack::setStack(ScriptStack* s)
{
    mStack = s;
}

ScriptStack* ScriptStack::getStack()
{
    return mStack;
}

void ScriptStack::setCall(ScriptCallStatement* call)
{
    mCall = call;
}

ScriptCallStatement* ScriptStack::getCall()
{
    return mCall;
}

void ScriptStack::setWarp(ScriptWarpStatement* warp)
{
    mWarp = warp;
}

ScriptWarpStatement* ScriptStack::getWarp()
{
    return mWarp;
}

void ScriptStack::setArguments(ExValueList* args)
{
    delete mArguments;
    mArguments = args;
}

ExValueList* ScriptStack::getArguments()
{
    return mArguments;
}

void ScriptStack::setIterator(ScriptIteratorStatement* it)
{
    mIterator = it;
}

ScriptIteratorStatement* ScriptStack::getIterator()
{
    return mIterator;
}

void ScriptStack::setLabel(ScriptLabelStatement* it)
{
    mLabel = it;
}

ScriptLabelStatement* ScriptStack::getLabel()
{
    return mLabel;
}

void ScriptStack::setSaveStatement(ScriptStatement* it)
{
    mSaveStatement = it;
}

ScriptStatement* ScriptStack::getSaveStatement()
{
    return mSaveStatement;
}

ScriptStatement* ScriptStack::getWait()
{
	return mWait;
}

void ScriptStack::setWait(ScriptStatement* wait)
{
	mWait = wait;
}

Event* ScriptStack::getWaitEvent()
{
	return mWaitEvent;
}

void ScriptStack::setWaitEvent(Event* e)
{
	mWaitEvent = e;
}

KernelEvent* ScriptStack::getWaitKernelEvent()
{
	return mWaitKernelEvent;
}

void ScriptStack::setWaitKernelEvent(KernelEvent* e)
{
	mWaitKernelEvent = e;
}

Function* ScriptStack::getWaitFunction()
{
	return mWaitFunction;
}

void ScriptStack::setWaitFunction(Function* e)
{
	mWaitFunction = e;
}

bool ScriptStack::isWaitBlock()
{
	return mWaitBlock;
}

void ScriptStack::setWaitBlock(bool b)
{
	mWaitBlock = b;
}

/**
 * Called by ScriptForStatement to add a track to the loop.
 */
void ScriptStack::addTrack(Track* t)
{
    if (mMax < MAX_TRACKS)
      mTracks[mMax++] = t;
}

/**
 * Called by ScriptForStatement to advance to the next track.
 */
Track* ScriptStack::nextTrack()
{
    Track* next = nullptr;

    if (mIndex < mMax) {
        mIndex++;
        next = mTracks[mIndex];
    }
    return next;
}

/**
 * Called by ScriptRepeatStatement to set the iteration count.
 */
void ScriptStack::setMax(int max)
{
	mMax = max;
}

int ScriptStack::getMax()
{
	return mMax;
}

/**
 * Called by ScriptRepeatStatement to advance to the next iteration.
 * Return true if we're done.
 */
bool ScriptStack::nextIndex()
{
	bool done = false;

	if (mIndex < mMax)
	  mIndex++;

	if (mIndex >= mMax)
	  done = true;

	return done;
}

/**
 * Determine the target track if we're in a For statement.
 * It is possible to have nested iterations, so search upward
 * until we find a For.  Nested fors don't make much sense, but
 * a nested for/repeat might be useful.  
 */
Track* ScriptStack::getTrack()
{
	Track* track = nullptr;
	ScriptStack* stack = this;
	ScriptStack* found = nullptr;

	// find the innermost For iteration frame
	while (found == nullptr && stack != nullptr) {
		ScriptIteratorStatement* it = stack->getIterator();
		if (it != nullptr && it->isFor())
		  found = stack;
		else
		  stack = stack->getStack();
	}

	if (found != nullptr && found->mIndex < found->mMax)
	  track = found->mTracks[found->mIndex];

	return track;
}

/**
 * Notify wait frames on the stack of the completion of a function.
 * 
 * Kludge for Wait switch, since we no longer have a the "fundamental"
 * command concept, assume that waiting for a function with
 * the SwitchEvent event type will end the wait on any of them,
 * need a better way to declare this.
 */
bool ScriptStack::finishWait(Function* f)
{
	bool finished = false;

	if (mWaitFunction != nullptr && 
		(mWaitFunction == f ||
		 (mWaitFunction->eventType == SwitchEvent && 
		  f->eventType == SwitchEvent))) {
			
		Trace(3, "Script end wait function %s\n", f->getName());
		mWaitFunction = nullptr;
		finished = true;
	}

	// maybe an ancestor is waiting
	// this should only happen if an async notification frame got pushed on top
	// of the wait frame
	// only return true if the current frame was waiting, not an ancestor
	// becuase we're still executing in the current frame and don't want to 
	// recursively call run() again
	if (mStack != nullptr) 
	  mStack->finishWait(f);

	return finished;
}

/**
 * Notify wait frames on the stack of the completion of an event.
 * Return true if we found this event on the stack.  This used when
 * canceling events so we can emit some diagnostic messages.
 */
bool ScriptStack::finishWait(Event* e)
{
	bool finished = false;

	if (mWaitEvent == e) {
		mWaitEvent = nullptr;
		finished = true;
	}

	if (mStack != nullptr) {
		if (mStack->finishWait(e))
		  finished = true;
	}

	return finished;
}

/**
 * Called as events are rescheduled into new events.
 * If we had been waiting on the old event, have to start wanting on the new.
 */
bool ScriptStack::changeWait(Event* orig, Event* neu)
{
	bool found = false;

	if (mWaitEvent == orig) {
		mWaitEvent = neu;
		found = true;
	}

	if (mStack != nullptr) {
		if (mStack->changeWait(orig, neu))
		  found = true;
	}

	return found;
}

/**
 * Notify wait frames on the stack of the completion of a thread event.
 */
bool ScriptStack::finishWait(KernelEvent* e)
{
	bool finished = false;
 
	if (mWaitKernelEvent == e) {
		mWaitKernelEvent = nullptr;
		finished = true;
	}

	if (mStack != nullptr) {
		if (mStack->finishWait(e))
		  finished = true;
	}

	return finished;
}

void ScriptStack::finishWaitBlock()
{
	mWaitBlock = false;
	if (mStack != nullptr)
	  mStack->finishWaitBlock();
}

/**
 * Cancel all wait blocks.
 *
 * How can there be waits on the stack?  Wait can only
 * be on the bottom most stack block, right?
 */
void ScriptStack::cancelWaits()
{
	if (mWaitEvent != nullptr) {
        // will si->getTargetTrack always be right nere?
        // can't get to it anyway, assume the Event knows
        // the track it is in
        Track* track = mWaitEvent->getTrack();
        if (track == nullptr) {
            Trace(1, "Wait event without target track!\n");
        }
        else {
            mWaitEvent->setScriptInterpreter(nullptr);

            EventManager* em = track->getEventManager();
            em->freeEvent(mWaitEvent);

            mWaitEvent = nullptr;
        }
    }

	if (mWaitKernelEvent != nullptr)
	  mWaitKernelEvent = nullptr;

	mWaitFunction = nullptr;
	mWaitBlock = false;

	if (mStack != nullptr) {
		mStack->cancelWaits();
		mStack->finishWaitBlock();
	}
}

/****************************************************************************
 *                                                                          *
 *                                 SCRIPT USE                               *
 *                                                                          *
 ****************************************************************************/

ScriptUse::ScriptUse(Symbol* s)
{
    mNext = nullptr;
    mParameter = s;
    mValue.setNull();
}

ScriptUse::~ScriptUse()
{
	ScriptUse *el, *next;

	for (el = mNext ; el != nullptr ; el = next) {
		next = el->getNext();
		el->setNext(nullptr);
		delete el;
	}
}

void ScriptUse::setNext(ScriptUse* next) 
{
    mNext = next;
}

ScriptUse* ScriptUse::getNext()
{
    return mNext;
}

Symbol* ScriptUse::getParameter()
{
    return mParameter;
}

ExValue* ScriptUse::getValue()
{
    return &mValue;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
