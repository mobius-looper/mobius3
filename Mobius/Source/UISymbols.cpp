/**
 * Definitions for Symbols related to the UI
 */

#include "model/Symbol.h"
#include "model/FunctionDefinition.h"
#include "model/ParameterProperties.h"
#include "model/ExValue.h"

#include "UISymbols.h"

void UISymbols::initialize()
{
    installDisplayFunction("UIParameterUp", UISymbolParameterUp);
    installDisplayFunction("UIParameterDown", UISymbolParameterDown);
    installDisplayFunction("UIParameterInc", UISymbolParameterInc);
    installDisplayFunction("UIParameterDec", UISymbolParameterDec);
    installDisplayFunction("ReloadScripts", UISymbolReloadScripts);
    installDisplayFunction("ReloadSamples", UISymbolReloadSamples);
    installDisplayFunction("ShowPanel", UISymbolShowPanel);
    
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
 * a Symbol that reserves the name and a ParameterProperties that
 * defines the characteristics of the parameter.
 *
 * There is some confusing overlap on the Symbol->level and
 * ParameterProperties->scope.  As we move away from UIParameter/FunctionDefinition
 * to the new ParameterProperties and FunctionProperties need to rethink this.
 * ParameterProperties is derived from UIParameter where scopes include things
 * like global, preset, setup, and UI.  This is not the same as Symbol->level but
 * in the case of UI related things they're the same since there are no UI level
 * parameters with Preset scope for example.  So it looks like duplication
 * but it's kind of not.
 */
void UISymbols::installDisplayParameter(const char* name, const char* label, int symbolId)
{
    ParameterProperties* p = new ParameterProperties();
    p->displayName = juce::String(label);
    p->type = TypeStructure;
    p->scope = ScopeUI;
    
    Symbol* s = Symbols.intern(name);
    s->behavior = BehaviorParameter;
    s->id = (unsigned char)symbolId;
    s->level = LevelUI;
    s->parameterProperties.reset(p);
}
