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
    int number = 0;
    bool midi = false;

};

class SessionTrackTable : public TypicalTable, public YanPopup::Listener,
                          public YanDialog::Listener
{
  public:

    const int ColumnName = 1;
    
    SessionTrackTable();
    ~SessionTrackTable();

    void initialize(class Provider* p);
    void load(class Provider* p, class Session* s);
    
    int getSelectedTrackNumber();
    int getTrackNumber(int row);

    bool isMidi(int row);

    void clear();

    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

    void yanPopupSelected(int id);
    void yanDialogClosed(class YanDialog* d, int button);
    
  private:
    
    class Provider* provider = nullptr;
    class Session* session = nullptr;
    juce::OwnedArray<class SessionTrackTableRow> tracks;
    int audioTracks = 0;
    int midiTracks = 0;
    
    YanPopup popup {this};
    YanDialog addAlert {this};
    YanDialog deleteAlert {this};
    YanDialog renameDialog {this};
    YanDialog bulkDialog {this};
    YanDialog bulkConfirm {this};

    YanInput newName {"New Name"};
    YanInput audioCount {"Audio Tracks"};
    YanInput midiCount {"Midi Tracks"};

    void reload();
    void countTracks();
    void selectRowByNumber(int n);
    
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
    
};
    
