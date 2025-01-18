/**
 * ConfigEditor for the Session.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/SessionConstants.h"
#include "../../model/Session.h"
#include "../../model/SystemConfig.h"
#include "../../model/MobiusConfig.h"
#include "../../model/UIConfig.h"
#include "../../Supervisor.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "../common/BasicTabs.h"

#include "SymbolTree.h"
#include "ParameterCategoryTree.h"
#include "ParameterForm.h"
#include "SessionTrackEditor.h"

#include "SessionEditor.h"

SessionEditor::SessionEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("SessionEditor");

    tabs.add("Parameters", &petab);

    trackEditor.reset(new SessionTrackEditor(s));
    tabs.add("Tracks", trackEditor.get());

    // the session isn't sensntive to user defined variables, so
    // we can build the tree view asap and don't have to reload it
    petab.loadSymbols(supervisor);
    
    trackEditor->loadSymbols();

    addAndMakeVisible(tabs);
}

SessionEditor::~SessionEditor()
{
}

Provider* SessionEditor::getProvider()
{
    // !! This should be done by ConfigEditor and everything in this
    // ecosystem should stop using Supervisor directly
    return supervisor;
}

void SessionEditor::prepare()
{
}

void SessionEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    tabs.setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditor overloads
//
//////////////////////////////////////////////////////////////////////

void SessionEditor::load()
{
    Session* src = supervisor->getSession();
    session.reset(new Session(src));
    revertSession.reset(new Session(src));
    
    loadSession();
}

Session* SessionEditor::getEditingSession()
{
    return session.get();
}


/**
 * Called by the Save button in the footer.
 */
void SessionEditor::save()
{
    // Session editing state is currently all held in the Fields rather than the
    // copied Session so we can just update the master and abandon the copy
    Session* master = supervisor->getSession();
    saveSession(master);
    supervisor->updateSession();

    session.reset(nullptr);
    revertSession.reset(nullptr);
}

/**
 * Throw away all editing state.
 */
void SessionEditor::cancel()
{
    session.reset(nullptr);
    revertSession.reset(nullptr);
}

void SessionEditor::revert()
{
    session.reset(new Session(revertSession.get()));
    
    loadSession();
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * Load the internal forms from the session now being edited.
 * Things must call back to getEditingSession.
 */
void SessionEditor::loadSession()
{
    petab.load();
    trackEditor->load();
}

/**
 * Deposit field results in this session, NOT the getEditingSession result.
 */
void SessionEditor::saveSession(Session* dest)
{
    petab.save(dest);
}

//////////////////////////////////////////////////////////////////////
//
// Parameters Tab
//
//////////////////////////////////////////////////////////////////////

SessionEditorParametersTab::SessionEditorParametersTab(SessionEditor* ed) : peditor(ed)
{
    addAndMakeVisible(&tree);
    addAndMakeVisible(&peditor);

    tree.setListener(this);
}

void SessionEditorParametersTab::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    int half = getHeight() / 2;
    tree.setBounds(area.removeFromLeft(half));
    peditor.setBounds(area);
}

void SessionEditorParametersTab::loadSymbols(Provider* p)
{
    tree.load(p->getSymbols(), "session");
}

void SessionEditorParametersTab::load()
{
    peditor.load();
}

void SessionEditorParametersTab::save(Session* dest)
{
    peditor.save(dest);
}

void SessionEditorParametersTab::symbolTreeClicked(SymbolTreeItem* item)
{
    SymbolTreeItem* container = item;
    
    // if this is a leaf node, go up to the parent and show the entire parent form
    if (item->getNumSubItems() == 0) {
        container = static_cast<SymbolTreeItem*>(item->getParentItem());
    }
    
    juce::Array<Symbol*>& symbols = container->getSymbols();
    if (symbols.size() == 0) {
        // this can happen for interior organizational nodes that don't
        // have symbols of their own, in this case, we could reset
        // the displayed form, or just continue displaying the last one
    }
    else {
        peditor.show(container->getName(), symbols);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Parameter Editor
//
//////////////////////////////////////////////////////////////////////

SessionParameterEditor::SessionParameterEditor(SessionEditor* ed)
{
    sessionEditor = ed;
}

void SessionParameterEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    for (auto form : forms)
      form->setBounds(area);
}

void SessionParameterEditor::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::pink);
    g.fillRect(0, 0, getWidth(), getHeight());
}

void SessionParameterEditor::show(juce::String category, juce::Array<Symbol*>& symbols)
{
    ParameterForm* form = formTable[category];

    if (form == nullptr) {
        Trace(2, "SPE: Creating form for category %s", category.toUTF8());
        
        form = new ParameterForm();
        forms.add(form);
        formTable.set(category, form);
        addAndMakeVisible(form);

        // todo: Read the form and/or tree definition from SystemConfig
        
        juce::String title = category;
        
        form->setTitle(title);
        form->add(symbols);
        
        currentForm = form;
        currentForm->load(sessionEditor->getEditingSession()->getGlobals());

        form->setBounds(getLocalBounds());
        // trouble getting this fleshed out dynamically
        form->resized();
        
    }
    else if (form == currentForm) {
        Trace(2, "SPE: Form already displayed for category %s", category.toUTF8());
    }
    else {
        Trace(2, "SPE: Displaying form for category %s", category.toUTF8());
        currentForm->setVisible(false);
        // probably need a refresh?
        form->setVisible(true);
        currentForm = form;
    }
}

void SessionParameterEditor::load()
{
    ValueSet* values = sessionEditor->getEditingSession()->getGlobals();
    for (auto form : forms)
      form->load(values);
}

void SessionParameterEditor::save(Session* dest)
{
    ValueSet* values = dest->ensureGlobals();
    for (auto form : forms)
      form->save(values);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
