/**
 * Symbol definitions for things handled by the UI
 *
 * There will be one UISymbols object inside Supervisor.
 * The only thing this really does is provide a method to
 * install the symbols, and maintain an owned array
 * of the UIParameter objects created at runtime.
 */

#pragma once

#include "model/UIParameter.h"

typedef enum {

    // functions
    UISymbolParameterUp = 1,
    UISymbolParameterDown,
    UISymbolParameterInc,
    UISymbolParameterDec,
    UISymbolReloadScripts,
    UISymbolReloadSamples,
    UISymbolShowPanel,
    
    // parameters
    UISymbolActiveLayout,
    UISymbolActiveButtons
    
} UISymbolId;

class UISymbols
{
  public:

    // fuck, We need to make UIParameter subclasses at runtime, but
    // SystemConstant expects a static const char* for the name and display name
    // it doesn't have to be a static constant as long as it is guaranteed to live
    // longer than The UIParameter does, but be safe
    // one way around this would be to give DisplayParameter it's own parallel name
    // model, but everything else still wants to call getName so would have
    // to overload that
    constexpr static const char* ActiveLayout = "activeLayout";
    constexpr static const char* ActiveLayoutLabel = "Active Layout";
    
    constexpr static const char* ActiveButtons = "activeButtons";
    constexpr static const char* ActiveButtonsLabel = "Active Buttons";
    
    UISymbols() {}
    ~UISymbols() {}

    void initialize();

  private:

    void installDisplayFunction(const char* name, int symbolId);
    void installDisplayParameter(const char* name, const char* label, int symbolId);
    
};
