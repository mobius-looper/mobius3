/**
 * SessionEditor subcomponent for editing track overrides.
 *
 * This is the more complex use of ParameterForm because it needs to
 * support the concepts of defaulting and occlusion.  
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
#include "../../model/ValueSet.h"
#include "../../model/ParameterSets.h"
#include "../../model/TreeForm.h"
#include "../../Provider.h"

#include "../parameter/SymbolTree.h"
#include "../parameter/ParameterForm.h"
#include "../parameter/ParameterTree.h"
#include "../parameter/ParameterFormCollection.h"

#include "SessionEditor.h"
#include "SessionTrackForms.h"

SessionTrackForms::SessionTrackForms()
{
}

SessionTrackForms::~SessionTrackForms()
{
}

void SessionTrackForms::initialize(Provider* p, SessionEditor* se, Session* s, Session::Track* def)
{
    provider = p;
    editor = se;
    session = s;
    sessionTrack = def;
    values = sessionTrack->ensureParameters();

    // experimenent with auto-save/load for user categories when the
    // same parameter can be in more than one form
    setDuplicateParameters(true);

    if (!lockingStyle)
      tree.setDraggable(true);

    if (def->type == Session::TypeAudio)
      tree.setTrackType(TrackTypeAudio);
    else if (def->type == Session::TypeMidi)
      tree.setTrackType(TrackTypeMidi);

    tree.setFilterNoOverride(true);
    
    tree.initializeDynamic(provider);

    // we get notifications of drops from the forms back to the tree
    // todo: this is also dependent on lockingStyle, right?
    tree.setDropListener(this);

    // before potentially loading forms, calculate the occlusion list
    // since that influsneces how things are displayed
    refreshOcclusionList();
    
    // this wants a ValueSet but we use a Refresher style so it
    // isn't needed during initialization
    // actually it is if you want to use the duplicateParameters option
    forms.initialize(this, values);
 
    // auto select the first tree node
    tree.selectFirst();

    // selecting the first node will load the first form
    //forms.refresh(this);
}

void SessionTrackForms::reload()
{
    forms.refresh(this);
}

void SessionTrackForms::save()
{
    forms.save(values);
}

void SessionTrackForms::cancel()
{
    forms.cancel();
    session = nullptr;
    sessionTrack = nullptr;
    values = nullptr;
}

void SessionTrackForms::decacheForms()
{
    forms.decache();
}

/**
 * Session::Track parame ter forms are more complicated than Overlay forms though
 * the resulting ValueSet is similar.  These were originally designed to work like
 * overlay forms with drag-and-drop to move fields in and out.  It changed to
 * use full forms combined with the "defaulted" and "occluded" concepts which looks
 * better for the user.
 *
 * A field that is defaulted is one that has no value in the track parameter map so
 * the effective value defaults to what is in the session.  Defaulting can be turned
 * on and off.
 *
 * A field that is occluded may have a value or be defaulted, but it is effectively
 * unused because a track overlay is in place that will hide that parameter.
 * Occlusion cannot be turned off manually, the user needs to go remove or change
 * the track overlay in the session.
 
 * Like other tree forms, the fields in each form are limited by the tree nodes
 * that appear within this category.
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
        
        // to get the title, have to get the TreeForm
        // see method comments for why this sucks
        TreeForm* formdef = getTreeForm(provider, formName);
        if (formdef != nullptr)
          form->setTitle(formdef->title);

        // this notifies us of drops into the form which we don't actually need
        // but also clicks which we do need
        form->setListener(this);

        // todo: form title

        // We can get the symbols by iterating over the children, but
        // won't the parent node already have a nice Array<Symbol> we could use instead?
        for (int i = 0 ; i < parent->getNumSubItems() ; i++) {
            SymbolTreeItem* item = static_cast<SymbolTreeItem*>(parent->getSubItem(i));
            Symbol* s = item->getSymbol();
            if (s == nullptr) {
                Trace(1, "SessionTrackForms: Tree node without symbol %s",
                      item->getName().toUTF8());
            }
            else {
                ParameterProperties* props = s->parameterProperties.get();
                if (props == nullptr) {
                    Trace(1, "SessionTrackForms: Tree node had a non-parameter symbol %s",
                          s->getName());
                }
                else {
                    YanParameter* field = nullptr;
                    MslValue* v = values->get(s->name);
                    if (!lockingStyle) {
                        // sparse mode: only add it if we have it
                        // OR if it is flagged as noDefault
                        if (v != nullptr || props->noDefault) {
                            field = new YanParameter(s->getDisplayName());
                            field->setDragDescription(s->name);
                            field->init(provider, s);
                            form->add(field);
                        }
                    }
                    else {
                        field = new YanParameter(s->getDisplayName());
                        field->init(provider, s);
                        // this is weird, should move the listener sensitivity
                        // up here?, or just have a flag that tells the form
                        // the label is sensitive
                        field->setLabelListener(form);
                        form->add(field);
                        if (v == nullptr && !props->noDefault)
                          field->setDefaulted(true);
                    }

                    // if this is the track overlay parameter,
                    // be informed when it changes
                    if (field != nullptr && s->id == ParamTrackOverlay)
                      field->setListener(this);
                }
            }
        }
        // the form was not loaded during the build phase
        // since we have complex refresh processing so
        // need another refresh pass
        form->refresh(this);
    }
    return form;
}

/**
 * Here is where the magic happens...
 *
 * ParameterForm has been asked to refresh the field values and it
 * calls back here.  This can happen during the Create phase above
 * or randomly as various things happen in the session editor after creation.
 */
void SessionTrackForms::parameterFormRefresh(ParameterForm* f, YanParameter* p)
{
    (void)f;
    // determine where the value comes from
    ValueSet* src = values;
    if (lockingStyle && p->isDefaulted())
      src = session->ensureGlobals();

    // editor combines our occlusion table with the two others
    SessionOcclusions::Occlusion* o = editor->getOcclusion(p->getSymbol(), occlusions);
    if (o != nullptr) {
        p->load(&(o->value));
        p->setOccluded(true);
        juce::String tooltip = juce::String("Occluded by overlay ") + o->source;
        p->setOcclusionSource(tooltip);
    }
    else {
        MslValue* v = src->get(p->getSymbol()->name);
        p->load(v);
        p->setOccluded(false);
    }
}

/**
 * Calculate the occlusion list for this track.
 */
void SessionTrackForms::refreshOcclusionList()
{
    occlusions.clear();
    // top-level editor has tools for this
    editor->gatherOcclusions(occlusions, values, ParamTrackOverlay);
}

/**
 * Here when the SessionEditor detected a change to either
 * the session overlay or the default track overlay.
 * Refresh the forms to pick up the changes.
 */
void SessionTrackForms::sessionOverlayChanged()
{
    forms.refresh(this);
}

/**
 * We install ourselves as a listener for the YanParameter field that
 * holds the track overlay.  Whenever this changes need to refresh the
 * occlusion list.
 *
 * NOTE WELL: You need to be very careful with this to avoid an infinite loop.
 * Refreshing the forms will cause them to have values placed in all
 * of the internal YanFields, if those fields trigger a Juce notification
 * when set programatically you'll end up back here, refresh the forms again
 * and it goes on forever.  YanInput in particular must NOT send notifications
 * when it has a value loaded, only when the user actually touches it.
 */
void SessionTrackForms::yanParameterChanged(YanParameter* p)
{
    // we only put this on one field but make sure
    Symbol* s = p->getSymbol();
    if (s->id != ParamTrackOverlay) {
        Trace(1, "SessionTrackForms: Unexpected YanParameter notification");
    }
    else {
        // have to move the value from the field back into the set
        MslValue v;
        p->save(&v);
        values->set(s->name, v);
        
        refreshOcclusionList();
        forms.refresh(this);
    }
}

/**
 * This will be called whenever the user clicks on a YanParameter field label.
 * If this is not lockingStyle we can ignore it.
 *
 * Simple toggle works well enough, but you could use the MouseEvent to
 * popup a selection menu on right click.
 */
void SessionTrackForms::parameterFormClick(ParameterForm* f, YanParameter* p, const juce::MouseEvent& e)
{
    (void)f;
    (void)e;
    if (lockingStyle) {
        // if this field has the noDefault flag set, then it can't be unlocked
        Symbol* s = p->getSymbol();
        ParameterProperties* props = s->parameterProperties.get();
        bool noLocking = (props != nullptr && props->noDefault);
        if (!noLocking)
          toggleParameterDefault(p);
    }
}

void SessionTrackForms::toggleParameterDefault(YanParameter* p)
{
    Symbol* s = p->getSymbol();

    if (p->isOccluded()) {
        // toggling is disabled while occluded, in part because
        // the occlusion color hides whether or not this is defaulted
        // or an override so nothing obvious happens besides making the
        // field editable, and it gives the impression that the field
        // can be meaningfully changed which it can't
    }
    else if (p->isDefaulted()) {
        p->setDefaulted(false);
        // occlusion doesn't change, but may as well re-use this
        parameterFormRefresh(nullptr, p);
    }
    else {
        // if they changed the value can save it so it will be restored if they
        // decide to immediately re-enable, this does however mean that you must filter
        // disabled field values on save()
        MslValue current;
        p->save(&current);
        values->set(s->name, current);

        p->setDefaulted(true);
        parameterFormRefresh(nullptr, p);
    }
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
            YanField* existing = form->find(s);
            if (existing == nullptr) {
                YanParameter* field = new YanParameter(s->getDisplayName());
                field->init(provider, s);
                field->setDragDescription(s->name);
                // if this is new there won't be a value here, but if they take
                // it out and put it back, it will be there
                field->load(values->get(s->name));
                form->add(field);
            }
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
