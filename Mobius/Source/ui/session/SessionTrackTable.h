/**
 * A table showing configured track summaries
 */

#pragma once

#include <JuceHeader.h>

#include "../script/TypicalTable.h"
#include "../common/YanPopup.h"
#include "../common/YanAlert.h"
#include "../common/YanDialog.h"
#include "../common/YanForm.h"
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
                          public YanAlert::Listener, public YanDialog::Listener
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
    void yanAlertSelected(class YanAlert* d, int id);
    void yanDialogOk(class YanDialog* d);
    
  private:
    
    class Provider* provider = nullptr;
    juce::OwnedArray<class SessionTrackTableRow> tracks;
    YanPopup popup {this};
    YanAlert deleteAlert {this};
    YanAlert addAlert {this};
    YanDialog renameDialog {this};
    //YanDialog bulkDialog;

    YanForm renameForm;
    YanInput renameInput {"New Name"};
    
    void menuAdd();
    void menuDelete();
    void menuRename();
    void menuBulk();
};
    
