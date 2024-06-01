/**
 * Definitions for Symbols related to the UI
 */

#include "model/Symbol.h"
#include "model/FunctionDefinition.h"
#include "model/UIParameter.h"
#include "model/ExValue.h"

#include "UISymbols.h"

void UISymbols::initialize()
{
    installDisplayFunction("UIParameterUp", UISymbolParameterUp);
    installDisplayFunction("UIParameterDown", UISymbolParameterDown);
    installDisplayFunction("UIParameterInc", UISymbolParameterInc);
    installDisplayFunction("UIParameterDec", UISymbolParameterDec);

    // while we have FunctionDefinition and UIParameter
    // objects defined, install them now too
    for (int i = 0 ; i < FunctionDefinition::Instances.size() ; i++) {
        FunctionDefinition* def = FunctionDefinition::Instances[i];
        Symbol* s = Symbols.intern(def->name);
        s->behavior = BehaviorFunction;
        // start them out in core, Mobuis can change that
        s->level = LevelCore;
        // we have an ordinal but that won't be used any more
        s->function = def;
    }
        
    for (int i = 0 ; i < UIParameter::Instances.size() ; i++) {
        UIParameter* def = UIParameter::Instances[i];
        Symbol* s = Symbols.intern(def->name);
        s->behavior = BehaviorParameter;
        s->level = LevelCore;
        s->parameter = def;
    }

    // runtime parameter experiment
    // I'd like to be able to create parameters at runtime
    // without needing static definition objects

    installDisplayParameter(UISymbols::ActiveLayout, UISymbols::ActiveLayoutLabel, UISymbolActiveLayout);
    installDisplayParameter(UISymbols::ActiveButtons, UISymbols::ActiveButtonsLabel, UISymbolActiveButtons);
}    

/**
 * A display function only needs a symbol.
 */
void UISymbols::installDisplayFunction(const char* name, int symbolId)
{
    Symbol* s = Symbols.intern(name);
    s->behavior = BehaviorFunction;
    s->id = (unsigned char)symbolId;
    s->level = LevelUI;
}

/**
 * Runtime defined parameters are defined by two things,
 * a Symbol that reserves the name and a DisplayParameter object
 * that hangs off the Symbol.
 */
void UISymbols::installDisplayParameter(const char* name, const char* label, int symbolId)
{
    DisplayParameter* p = new DisplayParameter();
    p->setName(name);
    p->setDisplayName(label);
    p->type = TypeStructure;
    p->scope = ScopeUI;
    // todo: lambdas to get/set the value would be sweet
    // right now Supervisor needs to intercept them
    parameters.add(p);
    
    Symbol* s = Symbols.intern(p->getName());
    s->behavior = BehaviorParameter;
    s->id = (unsigned char)symbolId;
    s->level = LevelUI;
    // need this when building the instant parameters list since it's wants a display name
    s->parameter = p;
}

//////////////////////////////////////////////////////////////////////
//
// DisplayParameter
//
// Subclass of UIParameter implementing the two pure virtuals so we can
// make one of these at runtime to represent the parameters related to the
// UI and handdled by Supervisor.
//
//////////////////////////////////////////////////////////////////////

void DisplayParameter::getValue(void* container, ExValue* value)
{
    (void)container;
    value->setNull();
}

void DisplayParameter::setValue(void* container, ExValue* value)
{
    (void)container;
    (void)value;
}
