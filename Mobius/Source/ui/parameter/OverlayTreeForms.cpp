/**
 * OverlayEditor subcomponent for editing one ValueSet.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/Symbol.h"
#include "../../model/ValueSet.h"
#include "../../model/TreeForm.h"
#include "../../Provider.h"

#include "SymbolTree.h"
#include "ParameterForm.h"
#include "ParameterTree.h"
#include "ParameterFormCollection.h"

#include "OverlayTreeForms.h"

OverlayTreeForms::OverlayTreeForms()
{
}

void OverlayTreeForms::initialize(Provider* p)
{
    provider = p;

    // exclude parameters that can't be in an overlay
    tree.setFilterNoOverlay(true);
    tree.setDraggable(true);
    tree.initializeDynamic(provider);

    // we get notifications of drops from the forms back to the tree
    tree.setDropListener(this);
    
    
    // this wants a ValueSet but we don't get that until load
    // rethink this interface, if we never have the ValueSet on
    // initialize then don't pass it
    forms.initialize(this, nullptr);
    forms.setFlatStyle(true);
}

void OverlayTreeForms::load(ValueSet* src)
{
    values = src;
    forms.load(src);
    // no, wait for the table to select something
//    tree.selectFirst();
}

void OverlayTreeForms::selectFirst()
{
    // only want to do this on first display
    tree.selectFirst();
}

void OverlayTreeForms::show()
{
    setVisible(true);
    if (!shownOnce) {
        selectFirst();
        shownOnce = true;
    }
}

void OverlayTreeForms::save(ValueSet* dest)
{
    forms.save(dest);
}

void OverlayTreeForms::cancel()
{
    forms.cancel();
    shownOnce = false;
}

void OverlayTreeForms::decacheForms()
{
    forms.decache();
    shownOnce = false;
}

/**
 * Overlay parameter forms are dynamic, they only show fields for the values
 * that are actually in the overlay.  Drag-and-drop is used to add or remove them.
 *
 * Like other tree forms, the fields are limited by the tree nodes
 * that appear within this category.
 *
 * This is a common dance that needs to be factored up to ParameterTreeForms.
 * What we can add here is a filter for the ValueSet.
 */
ParameterForm* OverlayTreeForms::parameterFormCollectionCreate(juce::String formName)
{
    ParameterForm* form = nullptr;

    // by convention we put the formName or "category" name on the item annotation
    // the same annotation will be set on the sub-items so this searcher
    // needs to stp at the highest level node that has this annotation
    SymbolTreeItem* parent = tree.findAnnotatedItem(formName);
    if (parent == nullptr) {
        Trace(1, "OverlayTreeForms: No tree node with annotation %s", formName.toUTF8());
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
        
        // allow symbols to be dragged in
        form->setListener(this);
        
        // todo: form title

        // so we can iterate over the children, but the parent node should also
        // have an Array of the child symbols as well, right?
        for (int i = 0 ; i < parent->getNumSubItems() ; i++) {
            SymbolTreeItem* item = static_cast<SymbolTreeItem*>(parent->getSubItem(i));
            Symbol* s = item->getSymbol();
            if (s == nullptr) {
                Trace(1, "OverlayTreeForms: Tree node without symbol %s",
                      item->getName().toUTF8());
            }
            else {
                // only add it if we have it
                MslValue* v = values->get(s->name);
                if (v != nullptr) {
                    YanParameter* field = new YanParameter(s->getDisplayName());
                    field->init(provider, s);
                    field->setDragDescription(s->name);
                    field->load(v);

                    // weridness now that we moved the field builder out of ParameterForm
                    // revisit the control flow on the label listener
                    field->setLabelListener(form);
                    
                    form->add(field);
                }
            }
        }
    }
    return form;
}

/**
 * Build a flat parameter form for all parameters in the overlay.
 * Tree is used to guide order and section headers.
 *
 * This may be altered after construction with drag and drop.
 */
ParameterForm* OverlayTreeForms::parameterFormCollectionCreateFlat()
{
    ParameterForm* form = new ParameterForm();

    // allow symbols to be dragged in
    form->setListener(this);
        
    // Each outer category becomes a section header
    SymbolTreeItem* root = tree.getRoot();
    for (int i = 0 ; i < root->getNumSubItems() ; i++) {
        SymbolTreeItem* category = static_cast<SymbolTreeItem*>(root->getSubItem(i));

        juce::Array<YanParameter*> fields;
        gatherFields(category, fields);

        if (fields.size() > 0) {
            form->addSection(category->getName(), category->getOrdinal());
            for (auto field : fields) {
                form->add(field);
            }
        }
    }
    
    return form;
}

void OverlayTreeForms::gatherFields(SymbolTreeItem* node, juce::Array<YanParameter*>& fields)
{
    if (node->getNumSubItems() > 0) {
        // a sub-category
        for (int i = 0 ; i < node->getNumSubItems() ; i++) {
            SymbolTreeItem* sub = static_cast<SymbolTreeItem*>(node->getSubItem(i));
            gatherFields(sub, fields);
        }
    }
    else {
        // a leaf
        Symbol* s = node->getSymbol();
        if (s != nullptr) {
            // only include if there is a value in this overlay
            if (values->get(s->name) != nullptr) {

                YanParameter* field = new YanParameter(s->getDisplayName());
                field->init(provider, s);
                field->setDragDescription(s->name);
                field->load(values->get(s->name));
                
                fields.add(field);

                // this is the crucial bit for proper ordering when
                // a random parameter is dropped into the form later
                // the tree defines the order
                field->setOrdinal(node->getOrdinal());
            }
        }
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
void OverlayTreeForms::parameterFormDrop(ParameterForm* form, juce::String drop)
{
    if (drop.startsWith(ParameterTree::DragPrefix)) {

        // the drag started from the tree, we get to add a field
        juce::String sname = drop.fromFirstOccurrenceOf(ParameterTree::DragPrefix, false, false);
        Symbol* s = provider->getSymbols()->find(sname);
        if (s == nullptr)
          Trace(1, "OverlayTreeForms: Invalid symbol name in drop %s", sname.toUTF8());
        else {
            YanField* existing = form->find(s);
            if (existing == nullptr) {

                YanParameter* field = new YanParameter(s->getDisplayName());
                field->init(provider, s);
                field->setDragDescription(s->name);
                field->load(values->get(s->name));

                // drop into a flat form is much more complex, though if you ever
                // want SessionTrackForms to use DnD rather than field locking it
                // should do the same thing

                // flatStyle is not derfined out here, we unconditionally set it
                // in initialize(), but if you ever want to have view alternates, then
                // test it here
                bool flatStyle = true;
                if (flatStyle) {
                    insertOrderedField(form, field, s);
                }
                else {
                    // yeah no, this should the the same tree-guided ordering
                    // rather than just appending
                    form->add(field);
                }
            }
        }
    }
    else if (drop.startsWith(YanFieldLabel::DragPrefix)) {
        // the drag stopped over the form itself
        // this is where we could support field reordering
        Trace(2, "OverlayTreeForms: Form drop unto itself");
    }
    else {
        Trace(2, "OverlayTreeForms: Unknown drop identifier %s", drop.toUTF8());
    }
}

/**
 * A Symbol has just been dropped onto a flat form and we need to figure out
 * where it goes, adding a category if necessary.  Get it working out here
 * then decide what should be moved down a layer into ParameterForm or YanForm.
 *
 * Locate the SymbolTreeNode for this symbol in the tree.
 * Get the parent category and insert an ordered category section into the form if necessary.
 * Once the category section exists, insert the field within the category fields in tree order.
 */
void OverlayTreeForms::insertOrderedField(ParameterForm* form, YanParameter* field, Symbol* symbol)
{
    SymbolTreeItem* item = tree.find(symbol);
    if (item == nullptr) {
        Trace(1, "OverlayTreeForms: No tree node for symbol %s", symbol->getName());
    }
    else {
        SymbolTreeItem* category = item->getParent();
        YanSection* section = form->findSection(category->getName());
        if (section == nullptr)
          section = form->insertOrderedSection(category->getName(), category->getOrdinal());

        form->insertOrderedField(section, field);
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
void OverlayTreeForms::dropTreeViewDrop(DropTreeView* srctree, const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)srctree;
    juce::String drop = details.description.toString();

    if (drop.startsWith(YanFieldLabel::DragPrefix)) {
        // the drag started from the form
        juce::String sname = drop.fromFirstOccurrenceOf(YanFieldLabel::DragPrefix, false, false);
        Symbol* s = provider->getSymbols()->find(sname);
        if (s == nullptr)
          Trace(1, "OverlayTreeForms: Invalid symbol name in drop %s", sname.toUTF8());
        else {
            // this can only have come from the currently displayed form
            ParameterForm* form = forms.getCurrentForm();
            if (form == nullptr) {
                Trace(1, "OverlayTreeForms: Drop from a form that wasn't ours %s", s->getName());
            }
            else {
                if (!form->remove(s))
                  Trace(1, "OverlayTreeForms: Problem removing symbol from form %s", s->getName());

                // The SessionEditor uses "locking" fields that are setDefaulted to indiciate
                // that they are no longer used by this form.  ParameterForm tests that to see
                // whether to remove the value from the ValueSet on save
                // since DnD style tree forms remove the ParameterField it can't use the
                // same mechanism.  You have to remove the value now
                // this does however mean that if you drag the field back onto the form the
                // previous value is lost
                values->remove(s->name);

                // todo: also remove the section heacer if there is nothing
                // else in this section
            }
        }
    }
    else if (drop.startsWith(ParameterTree::DragPrefix)) {
        // parameter tree is dragging onto itself
        // in this use of SymbolTree, reordering items is not allowed
        Trace(2, "OverlayTreeForms: Tree drop unto itself");
    }
    else {
        Trace(2, "OverlayTreeForms: Unknown drop identifier %s", drop.toUTF8());
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
