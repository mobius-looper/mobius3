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
 *   Call references between scripts in the ScriptLibrary are resolved.
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

#include "../../model/Trigger.h"
#include "../../model/MobiusConfig.h"
#include "../../model/ScriptConfig.h"
#include "../../model/UserVariable.h"
#include "../../model/Symbol.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Export.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "Mode.h"
#include "Project.h"
#include "Script.h"
#include "Synchronizer.h"
#include "Track.h"
#include "Variable.h"

#include "ScriptCompiler.h"
#include "Script.h"
#include "Mem.h"

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
	NULL
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
	NULL
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
    mInternalVariable = NULL;
    mVariable = NULL;
    mParameter = NULL;
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

ScriptResolver::ScriptResolver(ExSymbol* symbol, Parameter* p)
{
	init(symbol);
    mParameter = p;
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
    else if (mInternalVariable != NULL) {
		mInternalVariable->getValue(si, value);
    }
	else if (mVariable != NULL) {
        UserVariables* vars = NULL;
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
        
        if (vars != NULL)
          vars->get(name, value);
	}
    else if (mParameter != NULL) {
        // reuse an export 
        Export* exp = si->getExport();

        if (mParameter->scope == PARAM_SCOPE_GLOBAL) {
            exp->setTrack(NULL);
            mParameter->getValue(exp, value); 
        }
        else {
            exp->setTrack(si->getTargetTrack());
            mParameter->getValue(exp, value);
        }
    }
    else if (mInterpreterVariable != NULL) {
        UserVariables* vars = si->getVariables();
        if (vars != NULL)
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
    mLiteral = NULL;
    mStackArg = 0;
    mInternalVariable = NULL;
    mVariable = NULL;
    mParameter = NULL;
}

const char* ScriptArgument::getLiteral()
{
    return mLiteral;
}

void ScriptArgument::setLiteral(const char* lit) 
{
	mLiteral = lit;
}

Parameter* ScriptArgument::getParameter()
{
    return mParameter;
}

bool ScriptArgument::isResolved()
{
	return (mStackArg > 0 ||
			mInternalVariable != NULL ||
			mVariable != NULL ||
			mParameter != NULL);
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
    mInternalVariable = NULL;
    mVariable = NULL;
    mParameter = NULL;

    if (mLiteral != NULL) {

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
				if (mInternalVariable == NULL) {
                    if (block == NULL)
                      Trace(1, "ScriptArgument::resolve has no block!\n");
                    else {
                        mVariable = block->findVariable(ref);
                        if (mVariable == NULL) {
                            mParameter = m->getParameter(ref);
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
    else if (mInternalVariable != NULL) {
        mInternalVariable->getValue(si, value);
    }
	else if (mVariable != NULL) {
        UserVariables* vars = NULL;
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

        if (vars != NULL)
          vars->get(name, value);
	}
    else if (mParameter != NULL) {
        Export* exp = si->getExport();
        if (mParameter->scope == PARAM_SCOPE_GLOBAL) {
            exp->setTrack(NULL);
            mParameter->getValue(exp, value); 
        }
        else {
            exp->setTrack(si->getTargetTrack());
            mParameter->getValue(exp, value);
        }
    }
    else if (mLiteral != NULL) { 
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
	else if (mInternalVariable != NULL) {
        const char* name = mInternalVariable->getName();
        char traceval[128];
        value->getString(traceval, sizeof(traceval));
        Trace(2, "Script %s: setting internal variable %s = %s\n", 
              si->getTraceName(), name, traceval);
		mInternalVariable->setValue(si, value);
    }
	else if (mVariable != NULL) {
        char traceval[128];
        value->getString(traceval, sizeof(traceval));

        UserVariables* vars = NULL;
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
        
        if (vars != NULL)
          vars->set(name, value);
	}
	else if (mParameter != NULL) {
        const char* name = mParameter->getName();
        char traceval[128];
        value->getString(traceval, sizeof(traceval));

        // can resuse this unless it schedules
        Action* action = si->getAction();
        if (mParameter->scheduled)
          action = si->getMobius()->cloneAction(action);

        action->arg.set(value);

        if (mParameter->scope == PARAM_SCOPE_GLOBAL) {
            Trace(2, "Script %s: setting global parameter %s = %s\n",
                  si->getTraceName(), name, traceval);
            action->setResolvedTrack(NULL);
            mParameter->setValue(action);
        }
        else {
            Trace(2, "Script %s: setting track parameter %s = %s\n", 
                  si->getTraceName(), name, traceval);
            action->setResolvedTrack(si->getTargetTrack());
            mParameter->setValue(action);
        }

        if (mParameter->scheduled)
          si->getMobius()->completeAction(action);
	}
    else if (mLiteral != NULL) {
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
    mNext = NULL;
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
    mParent = NULL;
    mName = NULL;
    mDeclarations = NULL;
	mStatements = NULL;
	mLast = NULL;
}

ScriptBlock::~ScriptBlock()
{
    // parent is not an ownership relationship, don't delete it

    delete mName;

	ScriptDeclaration* decl = NULL;
	ScriptDeclaration* nextDecl = NULL;
	for (decl = mDeclarations ; decl != NULL ; decl = nextDecl) {
		nextDecl = decl->getNext();
		delete decl;
	}

	ScriptStatement* stmt = NULL;
	ScriptStatement* nextStmt = NULL;

	for (stmt = mStatements ; stmt != NULL ; stmt = nextStmt) {
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
    if (decl != NULL) {
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
	if (a != NULL) {
		if (mLast == NULL) {
			mStatements = a;
			mLast = a;
		}
		else {
			mLast->setNext(a);
			mLast = a;
		}

        if (a->getParentBlock() != NULL)
          Trace(1, "ERROR: ScriptStatement already has a block!\n");
        a->setParentBlock(this);
	}
}

/**
 * Resolve referenes within the block.
 */
void ScriptBlock::resolve(Mobius* m)
{
    for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext())
      s->resolve(m);
}

/**
 * Resolve calls to other scripts within this block.
 */
void ScriptBlock::link(ScriptCompiler* comp)
{
    for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext())
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

    ScriptVariableStatement* found = NULL;

    for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext()) {

        if (s->isVariable()) {
            ScriptVariableStatement* v = (ScriptVariableStatement*)s;
            if (name == NULL || StringEqualNoCase(name, v->getName())) {
                found = v;
                break;
            }
        }
    }

    if (found == NULL) {
        ScriptBlock* top = mParent;
        while (top != NULL && top->getParent() != NULL)
          top = top->getParent();

        if (top != NULL)
          found = top->findVariable(name);
    }

    return found;
}

/**
 * Search for a Label statement.
 */
ScriptLabelStatement* ScriptBlock::findLabel(const char* name)
{
	ScriptLabelStatement* found = NULL;

	for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext()) {
		if (s->isLabel()) {
			ScriptLabelStatement* l = (ScriptLabelStatement*)s;
			if (name == NULL || StringEqualNoCase(name, l->getArg(0))) {
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
	ScriptProcStatement* found = NULL;

	for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext()) {
		if (s->isProc()) {
			ScriptProcStatement* p = (ScriptProcStatement*)s;
			if (name == NULL || StringEqualNoCase(name, p->getArg(0))) {
                found = p;
                break;
            }
        }
    }

    if (found == NULL) {
        ScriptBlock* top = mParent;
        while (top != NULL && top->getParent() != NULL)
          top = top->getParent();

        if (top != NULL)
          found = top->findProc(name);
    }

	return found;
}

/**
 * Search for the For/Repeat statement matching a Next.
 */
ScriptIteratorStatement* ScriptBlock::findIterator(ScriptNextStatement* next)
{
    ScriptIteratorStatement* found = NULL;

    for (ScriptStatement* s = mStatements ; s != NULL ; s = s->getNext()) {
		// loops can be nested so find the nearest one that isn't already
		// paired with a next statement
        if (s->isIterator() && ((ScriptIteratorStatement*)s)->getEnd() == NULL)
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
	ScriptStatement* found = NULL;
	int depth = 0;

	for (ScriptStatement* s = start->getNext() ; s != NULL && found == NULL ;
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
    mParentBlock = NULL;
	mNext = NULL;
	for (int i = 0 ; i < MAX_ARGS ; i++)
	  mArgs[i] = NULL;
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
	mArgs[psn] = NULL;
	if (arg != NULL && strlen(arg) > 0)
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
 * Called when the entire ScriptLibrary has been loaded and the
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
void ScriptStatement::xwrite(FILE* fp)
{
    fprintf(fp, "%s", getKeyword());

	for (int i = 0 ; mArgs[i] != NULL ; i++) 
      fprintf(fp, " %s", mArgs[i]);

	fprintf(fp, "\n");
}

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
	if (line != NULL) {

        int max = MAX_ARGS;
        if (toParse > 0) {
            max = argOffset + toParse;
			if (max > MAX_ARGS)
              max = MAX_ARGS;
		}

		while (*line && argOffset < max) {
			bool quoted = false;

			// skip preceeding whitespace
			while (*line && isspace(*line)) line++;

			if (*line == '"') {
				quoted = true;
				line++;
			}

			if (*line) {
				char* token = line;
				if (quoted)
				  while (*line && *line != '"') line++;
				else
				  while (*line && !isspace(*line)) line++;
			
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
    
    return NULL;
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

    return NULL;
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

    return NULL;
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

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                    END                                   *
 *                                                                          *
 ****************************************************************************/

ScriptEndStatement ScriptEndStatementObj {NULL, NULL};
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
    return NULL;
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
	ScriptStatement* next = NULL;

    Trace(2, "Script %s: cancel\n", si->getTraceName());

	if (mCancelWait) {
		// This only makes sense within a notification thread, in the main
		// thread we couldn't be in a wait state
		// !! Should we set a script local variable that can be tested
		// to tell if this happened?
		ScriptStack* stack = si->getStack();
		if (stack != NULL)
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
    if (stack != NULL)
      stack->cancelWaits();

    // will this work without a declaration?
    UserVariables* vars = si->getVariables();
    if (vars != NULL) {
        ExValue v;
        v.setString("true");
        vars->set("interrupted", &v);
    }

    return NULL;
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
	mExpression = NULL;

	// isolate the first argument representing the reference
	// to the thing to set, the remainder is an expression
	args = parseArgs(args, 0, 1);

    if (args == NULL) {
        Trace(1, "Malformed set statement, missing arguments\n");
    }
    else {
        // ignore = between the name and initializer
        char* ptr = args;
        while (*ptr && isspace(*ptr)) ptr++;
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
	if (mExpression != NULL) {
		ExValue v;
		mExpression->eval(si, &v);
		mName.set(si, &v);
	}
	return NULL;
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
    Parameter* p = mName.getParameter();
    if (p == NULL) {
        Trace(1, "ScriptUseStatement: Not a parameter: %s\n", 
              mName.getLiteral());
    }   
    else { 
        si->use(p);
    }

    return ScriptSetStatement::eval(si);
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
	mName = NULL;
	mExpression = NULL;

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

	if (mName == NULL) {
		// first arg was the scope, parse another
        // see comments above about what we're doing here
        delete mArgs[0];
        mArgs[0] = nullptr;
        
		args = parseArgs(args, 0, 1);
		mName = mArgs[0];
	}

	// ignore = between the name and initializer
    if (args == NULL) {
        Trace(1, "Malformed Variable statement: missing arguments\n");
    }
    else {
        char* ptr = args;
        while (*ptr && isspace(*ptr)) ptr++;
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

	if (mName != NULL && mExpression != NULL) {
        
        UserVariables* vars = NULL;
        // sigh, don't have a trace sig that takes 3 strings 
        const char* tracemsg = NULL;

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

        if (vars == NULL)  {
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

    return NULL;
}

/****************************************************************************
 *                                                                          *
 *   							 CONDITIONAL                                *
 *                                                                          *
 ****************************************************************************/

ScriptConditionalStatement::ScriptConditionalStatement()
{
	mCondition = NULL;
}

ScriptConditionalStatement::~ScriptConditionalStatement()
{
	delete mCondition;
}

bool ScriptConditionalStatement::evalCondition(ScriptInterpreter* si)
{
	bool value = false;

	if (mCondition != NULL) {
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
    mStaticLabel = NULL;

	// the label
	args = parseArgs(args, 0, 1);
    
    if (args == NULL) {
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
    ScriptStatement* next = NULL;
	ExValue v;

	mLabel.get(si, &v);
	const char* label = v.getString();

	Trace(3, "Script %s: Jump %s\n", si->getTraceName(), label);

	if (evalCondition(si)) {
		if (mStaticLabel != NULL)
		  next = mStaticLabel;
		else {
			// dynamic resolution
            if (mParentBlock != NULL)
              next = mParentBlock->findLabel(label);

			if (next == NULL) {
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
    mElse = NULL;

	// ignore the first token if it is "if", it is a common error to
	// use "else if" rather than "else if"
	if (args != NULL) {
		while (*args && isspace(*args)) args++;
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
    ScriptStatement* next = NULL;
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
		while (next == NULL && clause != NULL) {
			if (clause->evalCondition(si)) {
				next = clause->getNext();
				if (next == NULL) {
					// malformed, don't infinite loop
                    Trace(1, "Script %s: ScriptIfStatement: malformed clause\n", si->getTraceName());
					next = ScriptEndStatement::Pseudo;
				}
			}
			else {
				ScriptStatement* nextClause = clause->getElse();
				if (nextClause == NULL) {
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
    return (mCondition != NULL) ? "Elseif" : "Else";
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
	return NULL;
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
    return NULL;
}

/****************************************************************************
 *                                                                          *
 *   							   ITERATOR                                 *
 *                                                                          *
 ****************************************************************************/

ScriptIteratorStatement::ScriptIteratorStatement()
{
	mEnd = NULL;
	mExpression = NULL;
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
	ScriptStatement* next = NULL;
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
			if (t->isFocusLock() || t == m->getTrack())
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
				if (t->getGroup() == group)
				  stack->addTrack(t);
			}
		}
	}
	else if (StartsWithNoCase(forspec, "outSyncMaster")) {
        Synchronizer* sync = m->getSynchronizer();
        Track* t = sync->getOutSyncMaster();
        if (t != NULL)
          stack->addTrack(t);
	}
	else if (StartsWithNoCase(forspec, "trackSyncMaster")) {
        Synchronizer* sync = m->getSynchronizer();
        Track* t = sync->getTrackSyncMaster();
        if (t != NULL)
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
				if (t != NULL)
				  stack->addTrack(t);
				digits = 0;
			}
		}
	}

	// if nothing was added, then skip it
	if (stack->getMax() == 0) {
		si->popStack();
		if (mEnd != NULL)
		  next = mEnd->getNext();

		if (next == NULL) {
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

	if (stack == NULL) {
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
		if (nextTrack != NULL)
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
	ScriptStatement* next = NULL;
	char spec[MIN_ARG_VALUE + 4];
	strcpy(spec, "");

	if (mExpression != NULL)
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
		if (mEnd != NULL)
		  next = mEnd->getNext();

 		if (next == NULL) {
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

	if (stack == NULL) {
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
	ScriptStatement* next = NULL;

	if (mExpression != NULL && mExpression->evalToBool(si)) {

		// push a block frame to hold iteration state
		// ScriptStack* stack = si->pushStack(this);
		(void)si->pushStack(this);
	}
	else {
		// while condition started off bad, just bad
		// treat this like an If with a false condition
		if (mEnd != NULL)
		  next = mEnd->getNext();

 		if (next == NULL) {
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

	if (stack == NULL) {
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
	else if (mExpression == NULL) {
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
    mIterator = NULL;
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
	if (mIterator != NULL)
	  mIterator->setEnd(this);
}

ScriptStatement* ScriptNextStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = NULL;

	if (mIterator == NULL) {
		// unmatched next, ignore
	}
	else if (!mIterator->isDone(si)) {
		next = mIterator->getNext();
	}
	else {
		// we should have an iteration frame on the stack, pop it
		ScriptStack* stack = si->getStack();
		if (stack != NULL && stack->getIterator() == mIterator)
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
    if (s == NULL)
      s = config->getSetup(ToInt(name));

    if (s != NULL) {
        // could pass ordinal here too...
		m->setActiveSetup(s->getName());
	}

    return NULL;
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
    if (p == NULL)
      p = config->getPreset(ToInt(name));

    if (p != NULL) {
		Track* t = si->getTargetTrack();
		if (t == NULL)
		  t = m->getTrack();

		// note that since we're in a script, we can set it immediately,
		// this is necessary if we have set statements immediately
		// following this that depend on the preset change
        t->setPreset(p);
    }

    return NULL;
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

    return NULL;
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
    Trace(2, "Script %s: InitPreset\n", si->getTraceName());

    // Not sure if this is necessary but we always started with the preset
    // from the active track then set it in the track specified in the
    // ScriptContext, I would expect them to be the same...

    Mobius* m = si->getMobius();
    Track* srcTrack = m->getTrack();
    Preset* p = srcTrack->getPreset();
    p->reset();

	// propagate this immediately to the track (avoid a pending preset)
	// so we can start calling set statements  
	Track* destTrack = si->getTargetTrack();
	if (destTrack == NULL)
	  destTrack = srcTrack;
    else if (destTrack != srcTrack)
      Trace(1, "Script %s: ScriptInitPresetStatement: Unexpected destination track\n",
            si->getTraceName());

    destTrack->setPreset(p);

    return NULL;
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
    return NULL;
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

    return NULL;
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

    return NULL;
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

    return NULL;
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
    ScriptStatement* next = NULL;

    const char* procname = si->actionArgs;
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
            if (block != NULL && block->getStatements() != NULL) {
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
	mProc = NULL;
	mScript = NULL;
    mExpression = NULL;

	// isolate the first argument representing the name of the
    // thing to call, the remainder is an expression
	args = parseArgs(args, 0, 1);

    if (args != NULL)
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
	if (mProc == NULL && mScript == NULL) {

		mScript = comp->resolveScript(mArgs[0]);
		if (mScript == NULL)
		  Trace(1, "Script %s: Unresolved call to %s\n", 
				comp->getScript()->getTraceName(), mArgs[0]);
	}
}

ScriptStatement* ScriptCallStatement::eval(ScriptInterpreter* si)
{
    ScriptStatement* next = NULL;

	if (mProc != NULL) {    
        ScriptBlock* block = mProc->getChildBlock();
        if (block != NULL && block->getStatements() != NULL) {
            // evaluate the argument list
            // !! figure out a way to pool ExNodes with ExValueLists 
            // in ScriptStack 
            
            ExValueList* args = NULL;
            if (mExpression != NULL)
              args = mExpression->evalToList(si);

            si->pushStack(this, si->getScript(), mProc, args);
            next = block->getStatements();
        }
	}
	else if (mScript != NULL) {

        ScriptBlock* block = mScript->getBlock();
        if (block != NULL && block->getStatements() != NULL) {

            // !! have to be careful with autoload from another "thread"
            // if we have a call in progress, need a reference count or 
            // something on the Script

            ExValueList* args = NULL;
            if (mExpression != NULL)
              args = mExpression->evalToList(si);

            si->pushStack(this, mScript, NULL, args);
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
	mScript = NULL;
    mExpression = NULL;

	// isolate the first argument representing the name of the
    // thing to call, the remainder is an expression
	args = parseArgs(args, 0, 1);

    if (args != NULL)
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
	if (mScript == NULL) {
		mScript = comp->resolveScript(mArgs[0]);
		if (mScript == NULL)
		  Trace(1, "Script %s: Unresolved call to %s\n", 
				comp->getScript()->getTraceName(), mArgs[0]);
	}
}

ScriptStatement* ScriptStartStatement::eval(ScriptInterpreter* si)
{
    (void)si;
    return NULL;
}

/****************************************************************************
 *                                                                          *
 *                                  BLOCKING                                *
 *                                                                          *
 ****************************************************************************/

ScriptBlockingStatement::ScriptBlockingStatement()
{
    mChildBlock = NULL;
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
    if (mChildBlock != NULL)
      mChildBlock->resolve(m);
}

/**
 * Since we are a blocking statement have to do recursive linking.
 */
void ScriptBlockingStatement::link(ScriptCompiler* compiler)
{
    if (mChildBlock != NULL)
      mChildBlock->link(compiler);
}

ScriptBlock* ScriptBlockingStatement::getChildBlock()
{
    if (mChildBlock == NULL)
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
    return NULL;
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
	return NULL;
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
    return NULL;
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
	return NULL;
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
    if (ptr != NULL) {
        mUp = true;
        args = ptr;
    }
    else {
        ptr = comp->skipToken(args, "down");
        if (ptr != NULL) {
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
    for (auto symbol : Symbols.getSymbols()) {
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

    if (mFunction != NULL && 
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
	mFunctionName = NULL;
	mFunction = NULL;
	mUp = false;
    mDown = false;
    mExpression = NULL;
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
    if (mFunction != NULL && 
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
	if (mFunction == NULL) {

        Script* callingScript = comp->getScript();

        if (mFunctionName == NULL) {
            Trace(1, "Script %s: missing function name\n", 
                  callingScript->getTraceName());
            Trace(1, "--> File %s line %ld\n",
                  callingScript->getFilename(), (long)mLineNumber);
        }
        else {
            // look for a script
            Script* calledScript = comp->resolveScript(mFunctionName);
            if (calledScript == NULL) {
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

    if (func  == NULL) {
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
        if (t != NULL) {
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

        if (mExpression == NULL) {
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
                if (list != NULL && list->size() > 0) {
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
        if (a->getEvent() != NULL) {
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
    return NULL;
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
    mExpression = NULL;
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
	for (int i = 0 ; WaitTypeNames[i] != NULL ; i++) {
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
    if (name != NULL) {
        int last = (int)strlen(name) - 1;
        if (last > 0 && name[last] == 's') {
            char buffer[128];
            strcpy(buffer, name);
            buffer[last] = '\0';
            name = buffer;
        }
    }

    for (int i = 0 ; WaitUnitNames[i] != NULL ; i++) {
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
    if (vars != NULL) {
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
            for (auto symbol : Symbols.getSymbols()) {
                if (symbol->coreFunction != nullptr &&
                    StringEqual(name, symbol->getName())) {
                    f = (Function*)symbol->coreFunction;
                    break;
                }
            }
            if (f == NULL)
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

    return NULL;
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
	e->setScript(si);
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
											  Preset::QUANTIZE_SUBCYCLE, 
											  current, time);
				}
                    break;

				case UNIT_CYCLE: {
					// wait for the start of a cycle after the current frame
					frame = getQuantizedFrame(loop, Preset::QUANTIZE_CYCLE, 
											  current, time);
				}
                    break;

				case UNIT_LOOP: {
					// wait for the start of a loop after the current frame
					frame = getQuantizedFrame(loop, Preset::QUANTIZE_LOOP, 
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
    if (mExpression != NULL) {
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
                                            Preset::QuantizeMode q, 
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

Script::Script(ScriptLibrary* env, const char* filename)
{
	init();
	mLibrary = env;
    setFilename(filename);
    mFunction = NEW1(RunScriptFunction, this);
}

void Script::init()
{
	mLibrary = NULL;
	mNext = NULL;
    mFunction = NULL;
    mName = NULL;
	mDisplayName = NULL;
	mFilename = NULL;
    mDirectory = NULL;

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

    mBlock = NULL;

	mReentryLabel = NULL;
	mSustainLabel = NULL;
	mEndSustainLabel = NULL;
	mClickLabel = NULL;	
    mEndClickLabel = NULL;
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

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

void Script::setLibrary(ScriptLibrary* env)
{
    mLibrary = env;
}

ScriptLibrary* Script::getLibrary()
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
	if (name == NULL) {
		if (mDisplayName == NULL) {
            if (mFilename != NULL) {
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
    mBlock = NULL;
	mReentryLabel = NULL;
	mSustainLabel = NULL;
	mEndSustainLabel = NULL;
	mClickLabel = NULL;
	mEndClickLabel = NULL;
}

ScriptBlock* Script::getBlock()
{
    if (mBlock == NULL)
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
    if (mBlock != NULL) {
        for (ScriptStatement* s = mBlock->getStatements() ; 
             s != NULL ; s = s->getNext()) {

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
	return (mSustainLabel != NULL || mEndSustainLabel != NULL);
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
	return (mClickLabel != NULL || mEndClickLabel != NULL);
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
    if (mBlock != NULL)
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
    if (mBlock != NULL)
      mBlock->link(comp);
}

/**
 * Can assume this is a full path 
 *
 * !!! This doesn't handle blocking statements, Procs won't write
 * properly.  Where is this used?
 */
void Script::xwrite(const char* filename)
{
	FILE* fp = fopen(filename, "w");
	if (fp == NULL) {
		// need a configurable alerting mechanism
		Trace(1, "Script %s: Unable to open file for writing: %s\n", 
              getDisplayName(), filename);
	}
	else {
        // !! write the options
        if (mBlock != NULL) {
            for (ScriptStatement* a = mBlock->getStatements() ; 
                 a != NULL ; a = a->getNext())
              a->xwrite(fp);
            fclose(fp);
        }
	}
}

//////////////////////////////////////////////////////////////////////
//
// ScriptLibrary
//
//////////////////////////////////////////////////////////////////////

ScriptLibrary::ScriptLibrary()
{
    mNext = NULL;
    mSource = NULL;
	mScripts = NULL;
}

ScriptLibrary::~ScriptLibrary()
{
	ScriptLibrary *el, *next;

    delete mSource;
    delete mScripts;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

ScriptLibrary* ScriptLibrary::getNext()
{
    return mNext;
}

void ScriptLibrary::setNext(ScriptLibrary* env)
{
    mNext = env;
}

ScriptConfig* ScriptLibrary::getSource()
{
    return mSource;
}

void ScriptLibrary::setSource(ScriptConfig* config)
{
    delete mSource;
    mSource = config;
}

Script* ScriptLibrary::getScripts()
{
	return mScripts;
}

void ScriptLibrary::setScripts(Script* scripts)
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
bool ScriptLibrary::isDifference(ScriptConfig* config)
{
    bool difference = false;

    if (mSource == NULL) {
        // started with nothing
        if (config != NULL && config->getScripts() != NULL)
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
Script* ScriptLibrary::getScript(Script* src)
{
    Script* found = NULL;

    for (Script* s = mScripts ; s != NULL ; s = s->getNext()) {
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
    // see comments in init() for why this has to be set NULL first
    mArguments = NULL;
	init();
}

/**
 * Called to initialize a stack frame when it is allocated
 * for the first time and when it is removed from the pool.
 * NOTE: Handling of mArguments is special because we own it, 
 * everything else is just a reference we can NULL.  The constructor
 * must set mArguments to NULL before calling this.
 */
void ScriptStack::init()
{
    mStack = NULL;
	mScript = NULL;
    mCall = NULL;
    mWarp = NULL;
	mIterator = NULL;
    mLabel = NULL;
    mSaveStatement = NULL;
	mWait = NULL;
	mWaitEvent = NULL;
	mWaitKernelEvent = NULL;
	mWaitFunction = NULL;
	mWaitBlock = false;
	mMax = 0;
	mIndex = 0;

	for (int i = 0 ; i < MAX_TRACKS ; i++)
	  mTracks[i] = NULL;

    // This is the only thing we own
    delete mArguments;
    mArguments = NULL;
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
    Track* next = NULL;

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
	Track* track = NULL;
	ScriptStack* stack = this;
	ScriptStack* found = NULL;

	// find the innermost For iteration frame
	while (found == NULL && stack != NULL) {
		ScriptIteratorStatement* it = stack->getIterator();
		if (it != NULL && it->isFor())
		  found = stack;
		else
		  stack = stack->getStack();
	}

	if (found != NULL && found->mIndex < found->mMax)
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

	if (mWaitFunction != NULL && 
		(mWaitFunction == f ||
		 (mWaitFunction->eventType == SwitchEvent && 
		  f->eventType == SwitchEvent))) {
			
		Trace(3, "Script end wait function %s\n", f->getName());
		mWaitFunction = NULL;
		finished = true;
	}

	// maybe an ancestor is waiting
	// this should only happen if an async notification frame got pushed on top
	// of the wait frame
	// only return true if the current frame was waiting, not an ancestor
	// becuase we're still executing in the current frame and don't want to 
	// recursively call run() again
	if (mStack != NULL) 
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
		mWaitEvent = NULL;
		finished = true;
	}

	if (mStack != NULL) {
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

	if (mStack != NULL) {
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
		mWaitKernelEvent = NULL;
		finished = true;
	}

	if (mStack != NULL) {
		if (mStack->finishWait(e))
		  finished = true;
	}

	return finished;
}

void ScriptStack::finishWaitBlock()
{
	mWaitBlock = false;
	if (mStack != NULL)
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
	if (mWaitEvent != NULL) {
        // will si->getTargetTrack always be right nere?
        // can't get to it anyway, assume the Event knows
        // the track it is in
        Track* track = mWaitEvent->getTrack();
        if (track == NULL) {
            Trace(1, "Wait event without target track!\n");
        }
        else {
            mWaitEvent->setScript(NULL);

            EventManager* em = track->getEventManager();
            em->freeEvent(mWaitEvent);

            mWaitEvent = NULL;
        }
    }

	if (mWaitKernelEvent != NULL)
	  mWaitKernelEvent = NULL;

	mWaitFunction = NULL;
	mWaitBlock = false;

	if (mStack != NULL) {
		mStack->cancelWaits();
		mStack->finishWaitBlock();
	}
}

/****************************************************************************
 *                                                                          *
 *                                 SCRIPT USE                               *
 *                                                                          *
 ****************************************************************************/

ScriptUse::ScriptUse(Parameter* p)
{
    mNext = NULL;
    mParameter = p;
    mValue.setNull();
}

ScriptUse::~ScriptUse()
{
	ScriptUse *el, *next;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
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

Parameter* ScriptUse::getParameter()
{
    return mParameter;
}

ExValue* ScriptUse::getValue()
{
    return &mValue;
}

/****************************************************************************
 *                                                                          *
 *   							 INTERPRETER                                *
 *                                                                          *
 ****************************************************************************/

ScriptInterpreter::ScriptInterpreter()
{
	init();
}

ScriptInterpreter::ScriptInterpreter(Mobius* m, Track* t)
{
	init();
	mMobius = m;
	mTrack = t;
}

void ScriptInterpreter::init()
{
	mNext = NULL;
    mNumber = 0;
    mTraceName[0] = 0;
	mMobius = NULL;
	mTrack = NULL;
	mScript = NULL;
    mUses = NULL;
    mStack = NULL;
    mStackPool = NULL;
	mStatement = NULL;
	mVariables = NULL;
    mTrigger = NULL;
    mTriggerId = 0;
	mSustaining = false;
	mClicking = false;
	mLastEvent = NULL;
	mLastKernelEvent = NULL;
	mReturnCode = 0;
	mPostLatency = false;
	mSustainedMsecs = 0;
	mSustainCount = 0;
	mClickedMsecs = 0;
	mClickCount = 0;
    mAction = NULL;
    mExport = NULL;
    strcpy(actionArgs, "");
}

ScriptInterpreter::~ScriptInterpreter()
{
	if (mStack != NULL)
	  mStack->cancelWaits();

    // do this earlier?
    restoreUses();

    delete mUses;
    delete mAction;
    delete mExport;

    // new: this was leaking
    delete mVariables;

    // the chain pointer here is getStack
    ScriptStack* stack = mStackPool;
    while (stack != nullptr) {
        ScriptStack* next = stack->getStack();
        stack->setStack(nullptr);
        delete stack;
        stack = next;
    }
}

void ScriptInterpreter::setNext(ScriptInterpreter* si)
{
	mNext = si;
}

ScriptInterpreter* ScriptInterpreter::getNext()
{
	return mNext;
}

void ScriptInterpreter::setNumber(int n) 
{
    mNumber = n;
}

int ScriptInterpreter::getNumber()
{
	return mNumber;
}

void ScriptInterpreter::setMobius(Mobius* m)
{
	mMobius = m;
}

Mobius* ScriptInterpreter::getMobius()
{
	return mMobius;
}

/**
 * Allocation an Action we can use when setting parameters.
 * We make one for function invocation too but that's more 
 * complicated and can schedule events.
 *
 * These won't have a ResolvedTarget since we've already
 * got the Parameter and we're calling it directly.
 */
Action* ScriptInterpreter::getAction()
{
    if (mAction == NULL) {
        mAction = mMobius->newAction();
        mAction->trigger = TriggerScript;
        
        // function action needs this for GlobalReset handling
        // I don't think Parameter actions do
        mAction->triggerOwner = this;
    }
    return mAction;
}

/**
 * Allocate an Export we can use when reading parameters.
 * We'll set the resolved track later.  This won't
 * have a ResolvedTarget since we've already got the
 * Parameter and will be using that directly.
 */
Export* ScriptInterpreter::getExport()
{
    if (mExport == NULL) {
        mExport = NEW1(Export, mMobius);
    }
    return mExport;
}

/**
 * Find a suitable name to include in trace messages so we have
 * some idea of what script we're dealing with.
 */
const char* ScriptInterpreter::getTraceName()
{
    if (mTraceName[0] == 0) {
        const char* name = "???";
        if (mScript != NULL)
          name = mScript->getDisplayName();

        snprintf(mTraceName, sizeof(mTraceName), "%d:", mNumber);
        int len = (int)strlen(mTraceName);

        AppendString(name, &mTraceName[len], MAX_TRACE_NAME - len - 1);
    }
    return mTraceName;
}

void ScriptInterpreter::setTrack(Track* m)
{
	mTrack = m;
}

Track* ScriptInterpreter::getTrack()
{
	return mTrack;
}

Track* ScriptInterpreter::getTargetTrack()
{
	Track* target = mTrack;
	if (mStack != NULL) {
		Track* t = mStack->getTrack();
		if (t != NULL)
		  target = t;
	}
	return target;
}

ScriptStack* ScriptInterpreter::getStack()
{
    return mStack;
}

bool ScriptInterpreter::isPostLatency()
{
	return mPostLatency;
}

void ScriptInterpreter::setPostLatency(bool b)
{
	mPostLatency = b;
}

int ScriptInterpreter::getSustainedMsecs()
{
	return mSustainedMsecs;
}

void ScriptInterpreter::setSustainedMsecs(int c)
{
	mSustainedMsecs = c;
}

int ScriptInterpreter::getSustainCount()
{
	return mSustainCount;
}

void ScriptInterpreter::setSustainCount(int c)
{
	mSustainCount = c;
}

bool ScriptInterpreter::isSustaining()
{
	return mSustaining;
}

void ScriptInterpreter::setSustaining(bool b)
{
	mSustaining = b;
}

int ScriptInterpreter::getClickedMsecs()
{
	return mClickedMsecs;
}

void ScriptInterpreter::setClickedMsecs(int c)
{
	mClickedMsecs = c;
}

int ScriptInterpreter::getClickCount()
{
	return mClickCount;
}

void ScriptInterpreter::setClickCount(int c)
{
	mClickCount = c;
}

bool ScriptInterpreter::isClicking()
{
	return mClicking;
}

void ScriptInterpreter::setClicking(bool b)
{
	mClicking = b;
}

/**
 * Save some things about the trigger that we can reference
 * later through ScriptVariables.
 *
 * TODO: Should we just clone the whole damn action?
 *
 * NEW: Yes, that would be handy, added support for requestId
 * which we want to use for tracking script execution and completion.
 * Also captured action arguments which is used to pass the Warp
 * statement entry point until we can rewrite the language
 * to support variable Calls.
 */
void ScriptInterpreter::setTrigger(Action* action)
{
    if (action == NULL) {
        mRequestId = 0;
        mTrigger = NULL;
        mTriggerId = 0;
        mTriggerValue = 0;
        mTriggerOffset = 0;
        strcpy(actionArgs, "");
    }
    else {
        mRequestId = action->requestId;
        mTrigger = action->trigger;
        mTriggerId = (int)(action->triggerId);
        mTriggerValue = action->triggerValue;
        mTriggerOffset = action->triggerOffset;
        CopyString(action->bindingArgs, actionArgs, sizeof(actionArgs));
    }
}

Trigger* ScriptInterpreter::getTrigger()
{
    return mTrigger;
}

int ScriptInterpreter::getTriggerId()
{
    return mTriggerId;
}

int ScriptInterpreter::getTriggerValue()
{
    return mTriggerValue;
}

int ScriptInterpreter::getTriggerOffset()
{
    return mTriggerOffset;
}

bool ScriptInterpreter::isTriggerEqual(Action* action)
{
    return (action->trigger == mTrigger && action->triggerId == mTriggerId);
}

void ScriptInterpreter::reset()
{
	mStatement = NULL;
    mTrigger = NULL;
    mTriggerId = 0;
	mSustaining = false;
	mClicking = false;
	mPostLatency = false;
	mSustainedMsecs = 0;
	mSustainCount = 0;
	mClickedMsecs = 0;
	mClickCount = 0;

	delete mVariables;
	mVariables = NULL;

    while (mStack != NULL)
      popStack();

	if (mScript != NULL) {
        ScriptBlock* block = mScript->getBlock();
        if (block != NULL)
          mStatement = block->getStatements();
    }

    // this?
    restoreUses();

    // lose these I suppose?
    mRequestId = 0;
    strcpy(actionArgs, "");
}

void ScriptInterpreter::setScript(Script* s, bool inuse)
{
	reset();
	mScript = s;

	// kludge, do not refesh if the script is currently in use
	if (!inuse && s->isAutoLoad()) {
        ScriptCompiler* comp = NEW(ScriptCompiler);
        comp->recompile(mMobius, s);
        delete comp;
    }

    ScriptBlock* block = s->getBlock();
    if (block != NULL)
      mStatement = block->getStatements();
}

/**
 * Formerly have been assuming that the Script keeps getting pushed
 * up the stack, but that's unreliable.  We need to be looking down the stack.
 */
Script* ScriptInterpreter::getScript()
{
	// find the first script on the stack
	Script* stackScript = NULL;

	for (ScriptStack* stack = mStack ; stack != NULL && stackScript == NULL ; 
		 stack = stack->getStack()) {
		stackScript = stack->getScript();
	}

	return (stackScript != NULL) ? stackScript : mScript;
}

bool ScriptInterpreter::isFinished()
{
	return (mStatement == NULL && !mSustaining && !mClicking);
}

/**
 * Return code accessor for the "returnCode" script variable.
 */
int ScriptInterpreter::getReturnCode()
{
	return mReturnCode;
}

void ScriptInterpreter::setReturnCode(int i) 
{
	mReturnCode = i;
}

/**
 * Add a use rememberance.  Only do this once.
 */
void ScriptInterpreter::use(Parameter* p)
{
    ScriptUse* found = NULL;

    for (ScriptUse* use = mUses ; use != NULL ; use = use->getNext()) {
        if (StringEqual(use->getParameter()->getName(), p->getName())) {
            found = use;
            break;
        }
    }

    if (found == NULL) {
        ScriptUse* use = NEW1(ScriptUse, p);
        ExValue* value = use->getValue();
        getParameter(p, value);
        use->setNext(mUses);
        mUses = use;
    }
}

/**
 * Restore the uses when the script ends.
 */
void ScriptInterpreter::restoreUses()
{
    for (ScriptUse* use = mUses ; use != NULL ; use = use->getNext()) {

        Parameter* p = use->getParameter();
        const char* name = p->getName();
        ExValue* value = use->getValue();
        char traceval[128];
        value->getString(traceval, sizeof(traceval));

        // can resuse this unless it schedules
        Action* action = getAction();
        if (p->scheduled)
          action = getMobius()->cloneAction(action);

        action->arg.set(value);

        if (p->scope == PARAM_SCOPE_GLOBAL) {
            Trace(2, "Script %s: restoring global parameter %s = %s\n",
                  getTraceName(), name, traceval);
            action->setResolvedTrack(NULL);
            p->setValue(action);
        }
        else {
            Trace(2, "Script %s: restoring track parameter %s = %s\n", 
                  getTraceName(), name, traceval);
            action->setResolvedTrack(getTargetTrack());
            p->setValue(action);
        }

        if (p->scheduled)
          getMobius()->completeAction(action);
	}

    delete mUses;
    mUses = NULL;

}

/**
 * Get the value of a parameter.
 */
void ScriptInterpreter::getParameter(Parameter* p, ExValue* value)
{
    Export* exp = getExport();

    if (p->scope == PARAM_SCOPE_GLOBAL) {
        exp->setTrack(NULL);
        p->getValue(exp, value); 
    }
    else {
        exp->setTrack(getTargetTrack());
        p->getValue(exp, value);
    }
}

/****************************************************************************
 *                                                                          *
 *                            INTERPRETER CONTROL                           *
 *                                                                          *
 ****************************************************************************/
/*
 * Methods called by Track to control the interpreter.
 * Other than the constructors, these are the only true "public" methods.
 * The methods called by all the handlers should be protected, but I 
 * don't want to mess with a billion friends.
 */

/**
 * Called by Track during event processing and at various points when a
 * function has been invoked.  Advance if we've been waiting on this function.
 *
 * Function may be null here for certain events like ScriptEvent.
 * Just go ahread and run.
 *
 * !! Need to sort out whether we want on the invocation of the
 * function or the event that completes the function.
 * 
 * !! Should this be waiting for event types?  The function
 * here could be an alternate ending which will confuse the script
 * 
 * !! The combined waits could address this though in an inconvenient
 * way, would be nice to have something like "Wait Switch any"
 * 
 */
void ScriptInterpreter::resume(Function* func)
{
	// if we have no stack, then can't be waiting
	if (mStack != NULL) {
		// note that we can't run() unless we were actually waiting, otherwise
		// we'll be here for most functions we actually *call* from the script
		// which causes a stack overflow
		if (mStack->finishWait(func))
		  run(false);
	}
}

/**
 * Called by KernelEvent handling when an event we scheduled is finished.
 * Note we don't run here since we're not in the audio interrupt thread.
 * Just remove the reference, the script will advance on the next interrupt.
 */
void ScriptInterpreter::finishEvent(KernelEvent* e)
{
	bool ours = false;

	if (mStack != NULL)
	  ours = mStack->finishWait(e);

	// Since we're dealing with another thread, it is possible
	// that the thread could notify us before the interpreter gets
	// to a "Wait thread", it is important that we NULL out the last
	// thread event so the Wait does't try to wait for an invalid event.

	if (mLastKernelEvent == e) {
		mLastKernelEvent = NULL;
		ours = true;
	}

	// If we know this was our event, capture the return code for
	// later use in scripts
	if (ours)
      mReturnCode = e->returnCode;
}

/**
 * Called by Loop after it processes any Event that has an attached
 * interpreter.  Check to see if we've met an event wait condition.
 * Can get here with ScriptEvents, but we will have already handled
 * those in the scriptEvent method below.
 */
void ScriptInterpreter::finishEvent(Event* event)
{
	if (mStack != NULL) {
		mStack->finishWait(event);

		// Make sure the last function state no longer references this event,
		// just in case there is another Wait last
		if (mLastEvent == event)
		  mLastEvent = NULL;

		// Kludge: Need to detect changes to the selected track and change
		// what we think the default track is.  No good way to encapsulate
		// this so look for specific function families.
		if (event->type == TrackEvent || event->function == GlobalReset) {
			// one of the track select functions, change the default track
			setTrack(mMobius->getTrack());
		}

		// have to run now too, otherwise we might invoke functions
		// that are supposed to be done in the current interrupt
		run(false);
	}
}

/**
 * Must be called when an event is canceled. So any waits can end.
 */
bool ScriptInterpreter::cancelEvent(Event* event)
{
	bool canceled = false;

	if (mStack != NULL)
	  canceled = mStack->finishWait(event);

	// Make sure the last function state no longer references this event,
	// just in case there is another Wait last.
 	if (mLastEvent == event)
	  mLastEvent = NULL;

	return canceled;
}

/**
 * Handler for a ScriptEvent scheduled in a track.
 */
void ScriptInterpreter::scriptEvent(Loop* l, Event* event)
{
    (void)l;
	if (mStack != NULL) {
		mStack->finishWait(event);
		// have to run now too, otherwise we might invoke functions
		// that are supposed to be done in the current interrupt
		run(false);
	}
}

/**
 * Called when a placeholder event has been rescheduled.
 * If there was a Wait for the placeholder event, switch the wait event
 * to the new event.
 */
void ScriptInterpreter::rescheduleEvent(Event* src, Event* neu)
{
	if (neu != NULL) {

		if (mStack != NULL) {
			if (mStack->changeWait(src, neu))
			  neu->setScript(this);
		}

		// this should only be the case if we did a Wait last, not
		// sure this can happen?
		if (mLastEvent == src) {
			mLastEvent = neu;
			neu->setScript(this);
		}
	}
}

/**
 * Called by Track at the beginning of each interrupt.
 * Factored out so we can tell if we're exactly at the start of a block,
 * or picking up in the middle.
 */
void ScriptInterpreter::run()
{
	run(true);
}

/**
 * Called at the beginning of each interrupt, or after
 * processing a ScriptEvent event.  Execute script statements in the context
 * of the parent track until we reach a wait state.
 *
 * Operations are normally performed on the parent track.  If
 * the script contains a FOR statement, the operations within the FOR
 * will be performed for each of the tracks specified in the FOR.  
 * But note that the FOR runs serially, not in parallel so if there
 * is a Wait statement in the loop, you will suspend in that track
 * waiting for the continuation event.
 */
void ScriptInterpreter::run(bool block)
{
	if (block && mStack != NULL)
	  mStack->finishWaitBlock();

	// remove the wait frame if we can
	checkWait();

	while (mStatement != NULL && !isWaiting()) {

        ScriptStatement* next = mStatement->eval(this);

		// evaluator may return the next statement, otherwise follow the chain
        if (next != NULL) {
            mStatement = next;
        }
        else if (mStatement == NULL) {
            // evaluating the last statement must have reset the script,
            // this isn't supposed to happen, but I suppose it could if
            // we allow scripts to launch other script threads and the
            // thread we launched immediately reset?
            Trace(1, "Script: Script was reset during execution!\n");
        }
        else if (!isWaiting()) {

			if (mStatement->isEnd())
			  mStatement = NULL;
			else
			  mStatement = mStatement->getNext();

			// if we hit an end statement, or fall off the end of the list, 
			// pop the stack 
			while (mStatement == NULL && mStack != NULL) {
				mStatement = popStack();
				// If we just exposed a Wait frame that has been satisfied, 
				// can pop it too.  This should only come into play if we just
				// finished an async notification.
				checkWait();
			}
		}
	}
    
    // !! if mStatement is NULL should we restoreUses now or wait
    // for Mobius to do it?  Could be some subtle timing if several
    // scripts use the same parameter

}

/**
 * If there is a wait frame on the top of the stack, and all the
 * wait conditions have been satisfied, remove it.
 */
void ScriptInterpreter::checkWait()
{
	if (isWaiting()) {
		if (mStack->getWaitFunction() == NULL &&
			mStack->getWaitEvent() == NULL &&
			mStack->getWaitKernelEvent() == NULL &&
			!mStack->isWaitBlock()) {

			// nothing left to live for...
			do {
				mStatement = popStack();
			}
			while (mStatement == NULL && mStack != NULL);
		}
	}
}

/**
 * Advance to the next ScriptStatement, popping the stack if necessary.
 */
void ScriptInterpreter::advance()
{
	if (mStatement != NULL) {

		if (mStatement->isEnd())
		  mStatement = NULL;
		else
          mStatement = mStatement->getNext();

        // when finished with a called script, pop the stack
        while (mStatement == NULL && mStack != NULL) {
			mStatement = popStack();
		}
    }
}

/**
 * Called when the script is supposed to unconditionally terminate.
 * Currently called by Track when it processes a GeneralReset or Reset
 * function that was performed outside the script.  Will want a way
 * to control this using script directives?
 */
void ScriptInterpreter::stop()
{
    // will also restore uses...
	reset();
	mStatement = NULL;
}

/**
 * Jump to a notification label.
 * These must happen while the interpreter is not running!
 */
void ScriptInterpreter::notify(ScriptStatement* s)
{
	if (s == NULL)
	  Trace(1, "Script %s: ScriptInterpreter::notify called without a statement!\n",
            getTraceName());

	else if (!s->isLabel())
	  // restict this to labels for now, though should support procs
	  Trace(1, "Script %s: ScriptInterpreter::notify called without a label!\n",
            getTraceName());

	else {
		pushStack((ScriptLabelStatement*)s);
		mStatement = s;
	}
}

/****************************************************************************
 *                                                                          *
 *                             INTERPRETER STATE                            *
 *                                                                          *
 ****************************************************************************/
/*
 * Methods that control the state of the interpreter called
 * by the stament evaluator methods.
 */

/**
 * Return true if any of the wait conditions are set.
 * If we're in an async notification return false so we can proceed
 * evaluating the notification block, leaving the waits in place.
 */
bool ScriptInterpreter::isWaiting()
{
	return (mStack != NULL && mStack->getWait() != NULL);
}

UserVariables* ScriptInterpreter::getVariables()
{
    if (mVariables == NULL)
      mVariables = NEW(UserVariables);
    return mVariables;
}

/**
 * Called after we've processed a function and it scheduled an event.
 * Since events may not be scheduled, be careful not to trash state
 * left behind by earlier functions.
 */
void ScriptInterpreter::setLastEvents(Action* a)
{
	if (a->getEvent() != NULL) {
		mLastEvent = a->getEvent();
		mLastEvent->setScript(this);
	}

	if (a->getKernelEvent() != NULL) {
		mLastKernelEvent = a->getKernelEvent();
		// Note that KernelEvents don't point back to the ScriptInterpreter
		// because the interpreter may be gone by the time the thread
		// event finishes.  Mobius will forward thread event completion
		// to all active interpreters.
	}
}

/**
 * Initialize a wait for the last function to complete.
 * Completion is determined by waiting for either the Event or
 * KernelEvent that was scheduled by the last function.
 */
void ScriptInterpreter::setupWaitLast(ScriptStatement* src)
{
	if (mLastEvent != NULL) {
		ScriptStack* frame = pushStackWait(src);
		frame->setWaitEvent(mLastEvent);
		// should we be setting this now?? what if the wait is canceled?
		mPostLatency = true;
	}
	else {
		// This can often happen if there is a "Wait last" after
		// an Undo or another function that has the scriptSync flag
		// which will cause an automatic wait.   Just ignore it.
	}
}

void ScriptInterpreter::setupWaitThread(ScriptStatement* src)
{
	if (mLastKernelEvent != NULL) {
		ScriptStack* frame = pushStackWait(src);
		frame->setWaitKernelEvent(mLastKernelEvent);
		// should we be setting this now?? what if the wait is canceled?
		mPostLatency = true;
	}
	else {
		// not sure if there are common reasons for this, but
		// if you try to wait for something that isn't there,
		// just return immediately
	}
}

/**
 * Allocate a stack frame, from the pool if possible.
 */
ScriptStack* ScriptInterpreter::allocStack()
{
    ScriptStack* s = NULL;
    if (mStackPool == NULL)
      s = NEW(ScriptStack);
    else {
        s = mStackPool;
        mStackPool = s->getStack();
        s->init();
    }
	return s;
}

/**
 * Push a call frame onto the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptCallStatement* call, 
										  Script* sub,
                                          ScriptProcStatement* proc,
                                          ExValueList* args)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
    s->setCall(call);
    s->setScript(sub);
    s->setProc(proc);
    s->setArguments(args);
    mStack = s;

    return s;
}

/**
 * Push a Warp frame onto the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptWarpStatement* warp,
                                          ScriptProcStatement* proc)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
    s->setWarp(warp);
    s->setProc(proc);
    mStack = s;

    return s;
}

/**
 * Push an iteration frame onto the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptIteratorStatement* it)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
	s->setIterator(it);
	// we stay in the same script
	if (mStack != NULL)
	  s->setScript(mStack->getScript());
	else
	  s->setScript(mScript);
	mStack = s;

    return s;
}

/**
 * Push a notification frame on the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptLabelStatement* label)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
	s->setLabel(label);
    s->setSaveStatement(mStatement);

	// we stay in the same script
	if (mStack != NULL)
	  s->setScript(mStack->getScript());
	else
	  s->setScript(mScript);
	mStack = s;

    return s;
}

/**
 * Push a wait frame onto the stack.
 * !! can't we consistently use pending events for waits?
 */
ScriptStack* ScriptInterpreter::pushStackWait(ScriptStatement* wait)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
    s->setWait(wait);

	// we stay in the same script
	if (mStack != NULL)
	  s->setScript(mStack->getScript());
	else
	  s->setScript(mScript);

    mStack = s;

    return s;
}

/**
 * Pop a frame from the stack.
 * Return the next statement to evaluate if we know it.
 */
ScriptStatement* ScriptInterpreter::popStack()
{
    ScriptStatement *next = NULL;

    if (mStack != NULL) {
        ScriptStack* parent = mStack->getStack();

        ScriptStatement* st = mStack->getCall();
		if (st != NULL) {
			// resume after the call
			next = st->getNext();
		}
        else if (mStack->getWarp() != nullptr) {
            // Warp immediately ends after the Proc
            // leave next null
        }
		else {
			st = mStack->getSaveStatement();
			if (st != NULL) {
				// must have been a asynchronous notification, return to the 
				// previous statement
				next = st;
			}
			else {
				st = mStack->getWait();
				if (st != NULL) {
					// resume after the wait
					next = st->getNext();
				}
				else {
					// iterators handle the next statement themselves
				}
			}
		}

        mStack->setStack(mStackPool);
        mStackPool = mStack;
        mStack = parent;
    }

    return next;
}

/**
 * Called by ScriptArgument and ScriptResolver to derive the value of
 * a stack argument.
 *
 * Recurse up the stack until we see a frame for a CallStatement,
 * then select the argument that was evaluated when the frame was pushed.
 */
void ScriptInterpreter::getStackArg(int index, ExValue* value)
{
	value->setNull();

    getStackArg(mStack, index, value);
}

/**
 * Inner recursive stack walker looking for args.
 */
void ScriptInterpreter::getStackArg(ScriptStack* stack,
                                            int index, ExValue* value)
{
    if (stack != NULL && index >= 1 && index <= MAX_ARGS) {
        ScriptCallStatement* call = stack->getCall();
		if (call == NULL) {
			// must be an iteration frame, recurse up
            getStackArg(stack->getStack(), index, value);
		}
		else {
            ExValueList* args = stack->getArguments();
            if (args != NULL) {
                // arg indexes in the script are 1 based
                ExValue* arg = args->getValue(index - 1);
                if (arg != NULL) {
                    // copy the stack argument to the return value
                    // if the arg contains a list (rare) the reference is
                    // transfered but it is not owned by the new value
                    value->set(arg);
                }
            }
        }
    }
}

/**
 * Run dynamic expansion on file path.
 * After expansion we prefix the base directory of the current script
 * if the resulting path is not absolute.
 *
 * TODO: Would be nice to have variables to get to the
 * installation and configuration directories.
 *
 * new: this was only used by the unit tests and made assumptions
 * about current working directory and where the script was loaded from
 * that conflicts with the new world order enforced by KernelEventHandler
 * and UnitTests.  Just leave the file unadorned and figure it out later.
 * The one thing that may make sense for the general user is relative
 * to the location of the script.  If you had a directory full of scripts
 * together with the files they loaded, you wouldn't have to use absolute paths
 * in the script.  But the new default of expecting them in the root directory
 * won't work.
 *
 * KernelEventHandler can't figure that out because the script location
 * is gone by the time it gets control of the KernelEvent.
 *
 * But we DON'T want relative path shanigans happening if we're in "unit test mode"
 * because KernelEventHandler/UnitTests will figure that out and it is different
 * than it used to be.
 *
 * Just leave the file alone for now and reconsider script-path-relative later
 * 
 */
void ScriptInterpreter::expandFile(const char* value, ExValue* retval)
{
	retval->setNull();

    // first do basic expansion
    expand(value, retval);
	
    // lobotomy of old code here, just leave it with the basic expansion
#if 0
    char* buffer = retval->getBuffer();
	int curlen = (int)strlen(buffer);

    if (curlen > 0 && !IsAbsolute(buffer)) {
		if (StartsWith(buffer, "./")) {
			// a signal to put it in the currrent working directory
			for (int i = 0 ; i <= curlen - 2 ; i++)
			  buffer[i] = buffer[i+2];
		}
		else {
            // relative to the script directory
			Script* s = getScript();
			const char* dir = s->getDirectory();
			if (dir != NULL) {
				int insertlen = (int)strlen(dir);
                int shiftlen = insertlen;
                bool needslash = false;
                if (insertlen > 0 && dir[insertlen] != '/' &&
                    dir[insertlen] != '\\') {
                    needslash = true;
                    shiftlen++;
                }

				int i;
				// shift down, need to check overflow!!
				for (i = curlen ; i >= 0 ; i--)
				  buffer[shiftlen+i] = buffer[i];
			
				// insert the prefix
				for (i = 0 ; i < insertlen ; i++)
				  buffer[i] = dir[i];


                if (needslash)
                  buffer[insertlen] = '/';
			}
		}
	}
#endif
}

/**
 * Called during statement evaluation to do dynamic reference expansion
 * for a statement argument, recursively walking up the call stack
 * if necessary.
 * 
 * We support multiple references in the string provided they begin with $.
 * Numeric references to stack arguments look like $1 $2, etc.
 * References to variables may look like $foo or $(foo) depending
 * on whether you have surrounding content that requries the () delimiters.
 */
void ScriptInterpreter::expand(const char* value, ExValue* retval)
{
    int len = (value != NULL) ? (int)strlen(value) : 0;
	char* buffer = retval->getBuffer();
    char* ptr = buffer;
    int localmax = retval->getBufferMax() - 1;
	int psn = 0;

	retval->setNull();

    // keep this terminated
    if (localmax > 0) *ptr = 0;
    
	while (psn < len && localmax > 0) {
        char ch = value[psn];
        if (ch != '$') {
            *ptr++ = ch;
			psn++;
            localmax--;
        }
        else {
            psn++;
            if (psn < len) {
                // assume that variables can't start with numbers
                // so if we find one it is a numeric argument ref
				// acctually this breaks for "8thsPerCycle" so it has
				// to be surrounded by () but that's an alias now
                char digit = value[psn];
                int index = (int)(digit - '0');
                if (index >= 1 && index <= MAX_ARGS) {
					ExValue v;
					getStackArg(index, &v);
					CopyString(v.getString(), ptr, localmax);
					psn++;
                }
                else {
                    // isolate the reference name
                    bool delimited = false;
                    if (value[psn] == '(') {
                        delimited = true;
                        psn++;
                    }
                    if (psn < len) {
                        ScriptArgument arg;
                        char refname[MIN_ARG_VALUE];
                        const char* src = &value[psn];
                        char* dest = refname;
                        // !! bounds checking
                        while (*src && !isspace(*src) && 
							   (delimited || *src != ',') &&
                               (!delimited || *src != ')')) {
                            *dest++ = *src++;
                            psn++;
                        }	
                        *dest = 0;
                        if (delimited && *src == ')')
						  psn++;
                            
                        // resolution logic resides in here
                        arg.resolve(mMobius, mStatement->getParentBlock(), refname);
						if (!arg.isResolved())
						  Trace(1, "Script %s: Unresolved reference: %s\n", 
                                getTraceName(), refname);	

						ExValue v;
						arg.get(this, &v);
						CopyString(v.getString(), ptr, localmax);
                    }
                }
            }

            // advance the local buffer pointers after the last
            // substitution
            int insertlen = (int)strlen(ptr);
            ptr += insertlen;
            localmax -= insertlen;
        }

        // keep it termianted
        *ptr = 0;
    }
}

/****************************************************************************
 *                                                                          *
 *                                 EXCONTEXT                                *
 *                                                                          *
 ****************************************************************************/

/**
 * An array containing names of variables that may be set by the interpreter
 * but do not need to be declared.
 */
const char* InterpreterVariables[] = {
    "interrupted",
    NULL
};

/**
 * ExContext interface.
 * Given the a symbol in an expression, search for a parameter,
 * internal variable, or stack argument reference with the same name.
 * If one is found return an ExResolver that will be called during evaluation
 * to retrieve the value.
 *
 * Note that this is called during the first evaluation, so we have
 * to get the current script from the interpreter stack.  
 *
 * !! Consider doing resolver assignment up front for consistency
 * with how ScriptArguments are resolved?
 * 
 */
ExResolver* ScriptInterpreter::getExResolver(ExSymbol* symbol)
{
	ExResolver* resolver = NULL;
	const char* name = symbol->getName();
	int arg = 0;

	// a leading $ is required for numeric stack argument references,
	// but must also support them for legacy symbolic references
	if (name[0] == '$') {
		name = &name[1];
		arg = ToInt(name);
	}

	if (arg > 0)
	  resolver = NEW2(ScriptResolver, symbol, arg);

    // next try internal variables
	if (resolver == NULL) {
		ScriptInternalVariable* iv = ScriptInternalVariable::getVariable(name);
		if (iv != NULL)
		  resolver = NEW2(ScriptResolver, symbol, iv);
	}
    
    // next look for a Variable in the innermost block
	if (resolver == NULL) {
        // we should only be called during evaluation!
        if (mStatement == NULL)
          Trace(1, "Script %s: getExResolver has no statement!\n", getTraceName());
        else {
            ScriptBlock* block = mStatement->getParentBlock();
            if (block == NULL)
              Trace(1, "Script %s: getExResolver has no block!\n", getTraceName());
            else {
                ScriptVariableStatement* v = block->findVariable(name);
                if (v != NULL)
                  resolver = NEW2(ScriptResolver, symbol, v);
            }
        }
    }

	if (resolver == NULL) {
		Parameter* p = mMobius->getParameter(name);
		if (p != NULL)
		  resolver = NEW2(ScriptResolver, symbol, p);
	}

    // try some auto-declared system variables
    if (resolver == NULL) {
        for (int i = 0 ; InterpreterVariables[i] != NULL ; i++) {
            if (StringEqualNoCase(name, InterpreterVariables[i])) {
                resolver = NEW2(ScriptResolver, symbol, name);
                break;
            }
        }
    }

	return resolver;
}

/**
 * ExContext interface.
 * Given the a symbol in non-built-in function call, search for a Proc
 * with a matching name.
 *
 * Note that this is called during the first evaluation, so we have
 * to get the current script from the interpreter stack.  
 * !! Consider doing resolver assignment up front for consistency
 * with how ScriptArguments are resolved?
 */
ExResolver* ScriptInterpreter::getExResolver(ExFunction* function)
{
    (void)function;
	return NULL;
}

KernelEvent* ScriptInterpreter::newKernelEvent()
{
    return mMobius->newKernelEvent();
}

/**
 * Send a KernelEvent off for processing, and remember it so we
 * can be notified when it completes.
 */
void ScriptInterpreter::sendKernelEvent(KernelEvent* e)
{
	// this is now the "last" thing we can wait for
	// do this before passing to the thread so we can get notified
	mLastKernelEvent = e;

    mMobius->sendKernelEvent(e);
}

/**
 * Shorthand for building and sending a common style of event.
 */
void ScriptInterpreter::sendKernelEvent(KernelEventType type, const char* arg)
{
    KernelEvent* e = newKernelEvent();
    e->type = type;
    e->setArg(0, arg);
    sendKernelEvent(e);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
