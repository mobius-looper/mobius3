
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/SymbolId.h"
#include "../../model/ParameterProperties.h"
#include "../../model/ValueSet.h"

#include "../common/YanForm.h"
#include "../common/YanParameter.h"

#include "../../Provider.h"

#include "YanParameterForm.h"

YanParameterForm::YanParameterForm(Provider* p)
{
    provider = p;
}

YanParameterForm::~YanParameterForm()
{
}

YanParameter* YanParameterForm::addField(SymbolId id)
{
    YanParameter* p = nullptr;
    
    Symbol* s = provider->getSymbols()->getSymbol(id);
    if (s == nullptr) {
        Trace(1, "YanParameter: Unable to map id to Symbol");
    }
    else {
        p = new YanParameter(s->getDisplayName());
        p->init(s);
        ownedFields.add(p);
        allFields.add(p);
        add(p);
    }
    return p;
}

void YanParameterForm::load(ValueSet* set)
{
    for (auto field : allFields) {
        Symbol* s = field->getSymbol();
        MslValue* v = nullptr;
        if (set != nullptr)
          v = set->get(s->name);
        field->load(v);
    }
}

void YanParameterForm::save(ValueSet* set)
{
    for (auto field : allFields) {
        Symbol* s = field->getSymbol();
        MslValue v;
        field->save(&v);
        set->set(s->name, v);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


