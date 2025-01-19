 
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "ParameterForm.h"
#include "SessionEditor.h"
#include "SessionTrackTable.h"
#include "SessionTrackTrees.h"
#include "SessionFormCollection.h"

#include "SessionTrackEditor.h"

SessionTrackEditor::SessionTrackEditor()
{
    tracks.reset(new SessionTrackTable());
    trees.reset(new SessionTrackTrees());
    forms.reset(new SessionFormCollection());
    
    addAndMakeVisible(tracks.get());
    addAndMakeVisible(trees.get());
    addAndMakeVisible(forms.get());
    
    tracks->setListener(this);

    currentTrack = 1;
}

SessionTrackEditor::~SessionTrackEditor()
{
}

void SessionTrackEditor::initialize(Provider* p)
{
    // temporarily save this until SessionTrackTable can
    // get everything it needs from just the hsession
    provider = p;
    
    tracks->initialize(p);
    trees->initialize(p);
}

void SessionTrackEditor::load(Session* s)
{
    // we are allowed to retain a pointer to this so track forms
    // can pull in new values as selections in the table and trees change
    session = s;
    
    // track configuration may change
    tracks->load(provider, s);
    
    // trees do not change once initialied

    // forms show the selected track
    loadForms(currentTrack);
}

/**
 * Now we have a problem.
 * SessionGlobalEditor was able to keep all editing state in the Forms
 * which could then just be dumped into the destination Session.
 *
 * TrackEditor is more complex because it has been dumping intermediate state
 * into the initial loaded Session as it transitions between tracks.  So some
 * of the editing state is in the initial Session and some is in the Forms.
 *
 * So we have a data merge problem.  You can't just replace the ValueSets in the
 * destination session with the ValueSets in the edited session because not everything
 * in the Session was displayed in the forms and some parameters may have changed
 * outside the SessionEditor, such as in the MixerEditor or something new in the future.
 *
 * The only things we are allowed to touch are the things managed by the forms in use.
 * So the transfer is somewhat awkward:
 *
 *     for each track
 *        load all forms from the initial session
 *        save all forms into the destination session
 *
 * The Track forms effectively perform the filtered merge.
 */
void SessionTrackEditor::save(Session* dest)
{
    // save editing state for the current track back to the intermediate session
    saveForms(currentTrack);

    // now load the intermediate session back into the forms one at a time
    // but save them to the destination session
    for (int i = 0 ; i < session->getTrackCount() ; i++) {
        Session::Track* t = session->getTrackByIndex(i);
        ValueSet* src = t->ensureParameters();
        forms->load(src);

        Session::Track* destTrack = dest->getTrackByNumber(t->number);
        if (destTrack == nullptr) {
            // this must be a new track we're supposed to create
            Trace(1, "SessionTrackEditor: Uneable to save new track forms");
        }
        else {
            ValueSet* destValues = destTrack->ensureParameters();
            forms->save(destValues);
        }
    }
}

void SessionTrackEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    tracks->setBounds(area.removeFromLeft(200));
    trees->setBounds(area.removeFromLeft(400));

    JuceUtil::dumpComponent(this);
}

/**
 * This is called when the selected row changes either by clicking on
 * it or using the keyboard arrow keys after a row has been selected.
 */
void SessionTrackEditor::typicalTableChanged(TypicalTable* t, int row)
{
    (void)t;
    //Trace(2, "SessionTrackEditor: Table changed row %d", row);

    if (tracks->isMidi(row)) {
        trees->showMidi(true);
    }
    else {
        trees->showMidi(false);
    }

    int newNumber = tracks->getSelectedTrackNumber();
    if (newNumber != currentTrack) {
        saveForms(currentTrack);
        loadForms(newNumber);
        currentTrack = newNumber;
    }
}

void SessionTrackEditor::symbolTreeClicked(SymbolTreeItem* item)
{
    (void)item;
    Trace(1, "SessionTrackEditor::symbolTreeClicked");
}

//////////////////////////////////////////////////////////////////////
//
// Internal State Transfer
//
//////////////////////////////////////////////////////////////////////

/**
 * Load the current set of editing forms with data from
 * the selected track.
 */
void SessionTrackEditor::loadForms(int number)
{
    Session::Track* trackdef = session->getTrackByNumber(number);
    if (trackdef == nullptr) {
        Trace(1, "SessionTrackEditor: Invalid track number %d", number);
    }
    else {
        forms->load(trackdef->ensureParameters());
    }
}

void SessionTrackEditor::saveForms(int number)
{    
    Session::Track* trackdef = session->getTrackByNumber(number);
    if (trackdef == nullptr) {
        Trace(1, "SessionTrackEditor: Invalid intermediate session track number %d", number);
    }
    else {
        forms->save(trackdef->ensureParameters());
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

        
    
    

