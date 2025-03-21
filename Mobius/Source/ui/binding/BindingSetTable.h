/**
 * A table showing ValueSets in a ParameterSet.
 */

#pragma once

#include <JuceHeader.h>

#include "../script/TypicalTable.h"
#include "../common/YanPopup.h"
#include "../common/YanDialog.h"

#include "../../Producer.h"

class BindingSetTableRow
{
  public:
    BindingSetTableRow() {
    }
    ~BindingSetTableRow() {
    }

    juce::String name;
    
};

class BindingSetTable : public TypicalTable,
                        public YanPopup::Listener,
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
    
    BindingSetTable(class BindingSetEditor* e);
    ~BindingSetTable();

    void load(class ParameterSets* sets);
    void reload();
    void clear();
    void cancel();
                
    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

    void mouseDown(const juce::MouseEvent& event) override;
    
    void yanPopupSelected(YanPopup* src, int id) override;
    void yanDialogClosed(YanDialog* d, int button) override;
    
  private:
    
    class NewBindingEditor* editor = nullptr;
    class BindingSets* sets = nullptr;
    
    juce::OwnedArray<class BindingSetTableRow> overlayRows;
    
    YanPopup rowPopup {this};
    YanPopup emptyPopup {this};
    
    YanDialog nameDialog {this};
    YanDialog deleteAlert {this};
    YanDialog confirmDialog {this};
    YanDialog errorAlert {this};
    
    YanInput newName {"New Name"};
    
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

    void showResult(juce::StringArray& errors);

};
    
