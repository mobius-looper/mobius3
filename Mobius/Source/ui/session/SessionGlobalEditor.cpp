/**
 * SessionEditor subcomponent for editing the global sesison parameters.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/StaticConfig.h"
#include "../../model/TreeForm.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "../parameter/SymbolTree.h"
#include "../parameter/ParameterForm.h"
#include "../parameter/ParameterTree.h"
#include "../parameter/ParameterFormCollection.h"

#include "SessionEditor.h"
#include "SessionGlobalEditor.h"

SessionGlobalEditor::SessionGlobalEditor()
{
}

void SessionGlobalEditor::initialize(Provider* p, SessionEditor* se)
{
    provider = p;
    editor = se;
    
    // this is used by the inherited symbolTreeClicked method
    // to generate form names when the tree is clicked if the clicked
    // node didn't specify one
    treeName = juce::String(TreeName);
    
    tree.initializeStatic(provider, treeName);

    // this wants a ValueSet but we don't get that until load
    forms.initialize(this, nullptr);

    // this actually doesn't do anything since cancel() will be called
    // soon after construction as part of the load() process to remove
    // lingering state from the last use
    //tree.selectFirst();
}

void SessionGlobalEditor::load(ValueSet* src)
{
    values = src;
    forms.load(src);

    // the load process will first cancel everything which
    // dumps any cached forms that may have been created
    // so have to wait until now to select the first one
    tree.selectFirst();
}

void SessionGlobalEditor::save(ValueSet* dest)
{
    forms.save(dest);
}

void SessionGlobalEditor::cancel()
{
    forms.cancel();
}

void SessionGlobalEditor::decacheForms()
{
    forms.decache();
}

/**
 * Global editor is guided by the static form definitions.
 */
ParameterForm* SessionGlobalEditor::parameterFormCollectionCreate(juce::String formName)
{
    ParameterForm* form = nullptr;
    
    StaticConfig* scon = provider->getStaticConfig();
    TreeForm* formdef = scon->getTreeForm(formName);
    if (formdef == nullptr) {
        Trace(1, "SessionGlobalEditor: Unknown form %s", formName.toUTF8());
    }
    else {
        form = new ParameterForm();

        juce::String title = formdef->title;
        if (title.length() == 0) title = formName;

        form->setTitle(title);

        form->build(provider, formdef);

        // ugh, this one builds a form from a TreeDefinition so we don't have
        // a hook into finding the YanParameter for the overlay like the others
        SymbolTable* symbols = provider->getSymbols();
        Symbol* s = symbols->getSymbol(ParamSessionOverlay);
        YanParameter* p = form->find(s);
        if (p == nullptr)
          Trace(1, "SessionGlobalEditor: Unable to find field for sessionOverlay");
        else
          p->setListener(this);

        form->load(values);
    }
    return form;
}

void SessionGlobalEditor::yanParameterChanged(YanParameter* p)
{
    // we only put this on one field but make sure
    Symbol* s = p->getSymbol();
    if (s->id != ParamSessionOverlay)
        Trace(1, "SessionTrackForms: Unexpected YanParameter notification");
    else {
        // have to move the value from the field back into the set
        MslValue v;
        p->save(&v);
        values->set(s->name, v);
        editor->overlayChanged();
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
