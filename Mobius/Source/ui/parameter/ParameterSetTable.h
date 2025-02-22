/**
 * A table showing ValueSets in a ParameterSet.
 */

#pragma once

#include <JuceHeader.h>

#include "../script/TypicalTable.h"
#include "../common/YanPopup.h"
#include "../common/YanDialog.h"

#include "../../Producer.h"

class ParameterSetTableRow
{
  public:
    ParameterSetTableRow() {
    }
    ~ParameterSetTableRow() {
    }

    juce::String name;
    
};

class ParameterSetTable : public TypicalTable, public YanPopup::Listener,
                          public YanDialog::Listener
{
  public:

    const int ColumnName = 1;
    
    typedef enum {
        DialogActivate = 1,
        DialogDeactivate,
        DialogCopy,
        DialogNew,
        DialogRename,
        DialogDelete
    } Dialog;
    
    ParameterSetTable(class Supervisor* s);
    ~ParameterSetTable();

    void load(class ParameterSets* set);
    void clear();

    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

    void mouseDown(const juce::MouseEvent& event) override;
    
    void yanPopupSelected(YanPopup* src, int id) override;
    void yanDialogClosed(YanDialog* d, int button) override;
    
  private:
    
    class Supervisor* supervisor = nullptr;
    class Producer* producer = nullptr;
    class ParameterSets* parameters = nullptr;
    
    juce::OwnedArray<class ParameterSetTableRow> sets;
    
    YanPopup rowPopup {this};
    YanPopup emptyPopup {this};
    
    YanDialog nameDialog {this};
    YanDialog deleteAlert {this};
    YanDialog confirmDialog {this};
    YanDialog errorAlert {this};
    
    YanInput newName {"New Name"};
    
    void reload();
    juce::String getSelectedName();
    
    void doActivate();
    void doDeactivate();
    void startNew();
    void startCopy();
    void startRename();
    void startDelete();

    void finishNew(int button);
    void finishCopy(int button);
    void finishRename(int button);
    void finishDelete(int button);

};
    
