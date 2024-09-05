
#include <JuceHeader.h>

#include "MslContext.h"
#include "MslEnvironment.h"
#include "MslCompilation.h"
#include "MslModel.h"
#include "MslSymbol.h"
#include "MslError.h"

/**
 * Link everything in a compilation unit.
 */
void MslLinker::link(MslContext* c, MslEnvironment* e, MslCompilation* u)
{
    context = c;
    environment = e;
    unit = u;

    // reset results in the unit
    unit->errors.clear();
    unit->collisions.clear();
    unit->unresolved.clear();

    link(unit->init.get());
    link(unit->body.get());

    for (auto func : unit->functions)
      link(func);

    // todo: figure out how to do variable iniitalizers
    // they either need to be part of the inititialization block
    // or we need to link them in place
}

void MslLinker::link(MslFunction* f)
{
    link(f->node.get());
}

void MslLinker::link(MslNode* node)
{
{
    // first link any chiildren
    for (auto child : node->children) {
        link(child);
        // todo: break on errors or keep going?
    }

    // now the hard part
    // only symbols need special processing right now
    if (node->isSymbol()) {
        MslSymbol* sym = static_cast<MslSymbol*>(node);
        link(sym);
    }
}

void MslLinker::link(MslSymbol* sym)
{
}
