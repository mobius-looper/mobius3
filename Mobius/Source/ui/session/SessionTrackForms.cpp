/**
 * OverlayEditor subcomponent for editing one ValueSet.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/Symbol.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "../parameter/SymbolTree.h"
#include "../parameter/ParameterForm.h"
#include "../parameter/ParameterTree.h"
#include "../parameter/ParameterFormCollection.h"

#include "SessionTrackForms.h"

SessionTrackForms::SessionTrackForms()
{
}

SessionTrackForms::~SessionTrackForms()
{
}

void SessionTrackForms::initialize(Provider* p, Session::Track* def)
{
    provider = p;
    sessionTrack = def;
    values = sessionTrack->ensureParameters();
    
    tree.setDraggable(true);

    if (def->type == Session::TypeAudio)
      tree.setTrackType(TrackTypeAudio);
    else if (def->type == Session::TypeMidi)
      tree.setTrackType(TrackTypeMidi);
    
    tree.initializeDynamic(provider);

    // we get notifications of drops from the forms back to the tree
    tree.setDropListener(this);
    
    
    // this wants a ValueSet but we don't get that until load
    // rethink this interface, if we never have the ValueSet on
    // initialize then don't pass it
    forms.initialize(p, this, nullptr);

    forms.load(values);
}

void SessionTrackForms::save()
{
    forms.save(values);
}

void SessionTrackForms::cancel()
{
    forms.cancel();
}

void SessionTrackForms::decacheForms()
{
    forms.decache();
}

/**
 * Session Track parameter forms are dynamic, they only show fields for the values
 * that are actually in the overlay.  Drag-and-drop is used to add or remove them.
 *
 * !! This is the same as OverlayTreeForms, need to share
 *
 * Like other tree forms, the fields are limited by the tree nodes
 * that appear within this category.
 *
 * This is a common dance that needs to be factored up to ParameterTreeForms.
 * What we can add here is a filter for the ValueSet.
 */
ParameterForm* SessionTrackForms::parameterFormCollectionCreate(juce::String formName)
{
    ParameterForm* form = nullptr;

    // by convention we put the formName or "category" name on the item annotation
    // the same annotation will be set on the sub-items so this searcher
    // needs to stp at the highest level node that has this annotation
    SymbolTreeItem* parent = tree.findAnnotatedItem(formName);
    if (parent == nullptr) {
        Trace(1, "SessionTrackForms: No tree node with annotation %s", formName.toUTF8());
    }
    else if (values == nullptr) {
        Trace(1, "OverlayTreeForm: No values.  Or morals probably either.");
    }
    else {
        form = new ParameterForm();
        // allow fields to gbe dragged out
        form->setDraggable(true);
        // allow symbols to be dragged in
        form->setListener(this);
        
        // todo: form title

        // so we can iterate over the children, but the parent node should also
        // have an Array of the child symbols as well, right?
        for (int i = 0 ; i < parent->getNumSubItems() ; i++) {
            SymbolTreeItem* item = static_cast<SymbolTreeItem*>(parent->getSubItem(i));
            Symbol* s = item->getSymbol();
            if (s == nullptr) {
                Trace(1, "SessionTrackForms: Tree node without symbol %s",
                      item->getName().toUTF8());
            }
            else {
                // only add it if we have it
                MslValue* v = values->get(s->name);
                if (v != nullptr)
                  form->add(provider, s, values);
            }
        }
    }
    return form;
}

//////////////////////////////////////////////////////////////////////
//
// Drag and Drop
//
//////////////////////////////////////////////////////////////////////

/**
 * Here when something is dropped onto one of the ParameterForms.
 * If this drop came from a ParameterTree, then add that symbol to the form
 * if it isn't there already.
 */
void SessionTrackForms::parameterFormDrop(ParameterForm* form, juce::String drop)
{
    if (drop.startsWith(ParameterTree::DragPrefix)) {

        // the drag started from the tree, we get to add a field
        juce::String sname = drop.fromFirstOccurrenceOf(ParameterTree::DragPrefix, false, false);
        Symbol* s = provider->getSymbols()->find(sname);
        if (s == nullptr)
          Trace(1, "SessionTrackForms: Invalid symbol name in drop %s", sname.toUTF8());
        else {
            // hmm, we don't necessarily need to pass the valueSet here since if this
            // is a new field, there shouldn't have been a value, but if they take it
            // out and put it back, this would restore the value
            form->add(provider, s, values);
        }
    }
    else if (drop.startsWith(YanFieldLabel::DragPrefix)) {
        // the drag stopped over the form itself
        // this is where we could support field reordering
        Trace(2, "SessionTrackForms: Form drop unto itself");
    }
    else {
        Trace(2, "SessionTrackForms: Unknown drop identifier %s", drop.toUTF8());
    }
}

/**
 * Here when something is dropped onto the ParameterTree.
 * If this drop came from a ParameterForm, then it is a signal that the field
 * should be removed.
 *
 * For some reason I decided to pass the entire DragAndDropTarget here, but
 * we only need the description, revisit.
 */
void SessionTrackForms::dropTreeViewDrop(DropTreeView* srctree, const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)srctree;
    juce::String drop = details.description.toString();

    if (drop.startsWith(YanFieldLabel::DragPrefix)) {
        // the drag started from the form
        juce::String sname = drop.fromFirstOccurrenceOf(YanFieldLabel::DragPrefix, false, false);
        Symbol* s = provider->getSymbols()->find(sname);
        if (s == nullptr)
          Trace(1, "SessionTrackForms: Invalid symbol name in drop %s", sname.toUTF8());
        else {
            // this can only have come from the currently displayed form
            ParameterForm* form = forms.getCurrentForm();
            if (form == nullptr) {
                Trace(1, "SessionTrackForms: Drop from a form that wasn't ours %s", s->getName());
            }
            else {
                if (!form->remove(s))
                  Trace(1, "SessionTrackForms: Problem removing symbol from form %s", s->getName());
            }
        }
    }
    else if (drop.startsWith(ParameterTree::DragPrefix)) {
        // parameter tree is dragging onto itself
        // in this use of SymbolTree, reordering items is not allowed
        Trace(2, "SessionTrackForms: Tree drop unto itself");
    }
    else {
        Trace(2, "SessionTrackForms: Unknown drop identifier %s", drop.toUTF8());
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
