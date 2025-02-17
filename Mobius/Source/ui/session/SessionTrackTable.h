/**
 * A table showing configured track summaries
 */

#pragma once

#include <JuceHeader.h>

#include "../script/TypicalTable.h"
#include "../common/YanPopup.h"
#include "../common/YanDialog.h"
#include "../common/YanField.h"

class SessionTrackTableRow
{
  public:
    SessionTrackTableRow() {
    }
    ~SessionTrackTableRow() {
    }

    juce::String name;
    bool midi = false;

};

class SessionTrackTable : public TypicalTable, public YanPopup::Listener,
                          public YanDialog::Listener,
                          public juce::DragAndDropTarget, public juce::DragAndDropContainer
{
  public:

    const int ColumnName = 1;
    
    SessionTrackTable();
    ~SessionTrackTable();

    void initialize(class Provider* p, class SessionTrackEditor* e);
    void load(class Provider* p, class Session* s);
    void reload();
    
    bool isMidi(int row);

    void clear();

    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

    void yanPopupSelected(class YanPopup* src, int id) override;
    void yanDialogClosed(class YanDialog* d, int button) override;

    // trying to listener for clicks in the table
    void mouseDown(const juce::MouseEvent& e) override;

    // reorder hacking
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDragEnter (const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDragMove (const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDragExit (const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails&) override;

    juce::var getDragSourceDescription (const juce::SparseSet<int>& selectedRows) override;
    
  private:
    
    class Provider* provider = nullptr;
    class SessionTrackEditor* editor = nullptr;
    class Session* session = nullptr;
    juce::OwnedArray<class SessionTrackTableRow> tracks;
    int audioTracks = 0;
    int midiTracks = 0;
    
    YanPopup rowPopup {this};
    YanPopup emptyPopup {this};
    YanDialog addAlert {this};
    YanDialog deleteAlert {this};
    YanDialog renameDialog {this};
    YanDialog bulkDialog {this};
    YanDialog bulkConfirm {this};

    YanInput newName {"New Name"};
    YanInput audioCount {"Audio Tracks"};
    YanInput midiCount {"Midi Tracks"};

    void countTracks();
    
    void startAdd();
    void startDelete();
    void startRename();
    void startBulk();
    void startBulkConfirm(int button);

    void finishAdd(int button);
    void finishDelete(int button);
    void finishRename(int button);
    void finishBulkConfirm(int button);
    void finishBulk(int button);

    // drag and drop hacking
    bool targetActive = false;
    bool moveActive = false;
    int lastInsertIndex = -1;
    int getDropRow(const juce::DragAndDropTarget::SourceDetails& details);
    bool doMove(int sourceRow, int dropRow);
    
};
    
