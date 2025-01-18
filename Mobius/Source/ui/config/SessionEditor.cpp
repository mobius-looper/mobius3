/**
 * ConfigEditor for the Session.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/SessionConstants.h"
#include "../../model/Session.h"
#include "../../model/MobiusConfig.h"
#include "../../model/UIConfig.h"
#include "../../Supervisor.h"
#include "../../Provider.h"

#include "../common/BasicTabs.h"
#include "../common/YanForm.h"
#include "../common/YanField.h"
#include "../common/SimpleRadio.h"

#include "../../sync/Transport.h"

#include "SymbolTree.h"
#include "ParameterCategoryTree.h"
#include "SessionEditorForm.h"

#include "SessionEditor.h"

SessionEditor::SessionEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("SessionEditor");
    render();
}

SessionEditor::~SessionEditor()
{
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
    
    petab.load(supervisor);
    
    loadSession();
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
 * Load a setup into the parameter fields
 */
void SessionEditor::loadSession()
{
    midiOut.setValue(session->getBool(SessionTransportMidiEnable));
    midiClocks.setValue(session->getBool(SessionTransportClocksWhenStopped));
}

void SessionEditor::saveSession(Session* dest)
{
    dest->setBool(SessionTransportMidiEnable, midiOut.getValue());
    dest->setBool(SessionTransportClocksWhenStopped, midiClocks.getValue());
}

//////////////////////////////////////////////////////////////////////
//
// Form Rendering
//
//////////////////////////////////////////////////////////////////////

void SessionEditor::render()
{
    transportForm.addSpacer();
    transportForm.add(&midiOut);
    transportForm.add(&midiClocks);
    
    tabs.add("Transport", &transportForm);
    tabs.add("Parameters", &petab);
    
    addAndMakeVisible(tabs);
}

//////////////////////////////////////////////////////////////////////
//
// Listneners
//
//////////////////////////////////////////////////////////////////////

/**
 * Respond to the track selection radio
 */
void SessionEditor::radioSelected(YanRadio* r, int index)
{
    (void)r;
    (void)index;
}

void SessionEditor::comboSelected(class YanCombo* c, int index)
{
    (void)c;
    (void)index;
    //Trace(2, "SessionEditor: Sync source selected %d", index);
}

void SessionEditor::inputChanged(class YanInput* input)
{
    (void)input;
    //Trace(2, "SessionEditor: Track count changed %d", input->getInt());
}

//////////////////////////////////////////////////////////////////////
//
// Parameters Tab
//
//////////////////////////////////////////////////////////////////////

SessionEditorParametersTab::SessionEditorParametersTab()
{
    addAndMakeVisible(&tree);
    addAndMakeVisible(&editor);

    tree.setListener(this);
}

void SessionEditorParametersTab::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    int half = getHeight() / 2;
    tree.setBounds(area.removeFromLeft(half));
    editor.setBounds(area);
}

void SessionEditorParametersTab::load(Provider* p)
{
    tree.load(p->getSymbols(), "session");
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
        editor.load(container->getName(), symbols);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Parameter Editor
//
//////////////////////////////////////////////////////////////////////

SessionParameterEditor::SessionParameterEditor()
{
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

void SessionParameterEditor::load(juce::String category, juce::Array<Symbol*>& symbols)
{
    SessionEditorForm* form = formTable[category];

    if (form != nullptr) {
        if (form == currentForm) {
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
    else {
        Trace(2, "SPE: Creating form for category %s", category.toUTF8());
        form = new SessionEditorForm();
        forms.add(form);
        formTable.set(category, form);

        addAndMakeVisible(form);
        form->setBounds(getLocalBounds());
        
        form->load(category, symbols);
        currentForm = form;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
