/**
 * A table showing BindingSets in a BindingSets.
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

    BindingSet* set = nullptr;
    
};

class BindingSetTable : public TypicalTable,
                        public YanPopup::Listener,
                        public YanDialog::Listener
{
  public:

    const int ColumnName = 1;
    
    typedef enum {
        DialogCopy = 1,
        DialogNew,
        DialogProperties,
        DialogDelete,
        DialogHelp
    } Dialog;
    
    BindingSetTable(class BindingEditor* e);
    ~BindingSetTable();

    void load(class BindingSets* sets);
    void refresh();
    void reload();
    void clear();
    void cancel();
                
    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

    void mouseDown(const juce::MouseEvent& event) override;
    
    void yanPopupSelected(YanPopup* src, int id) override;
    void yanDialogClosed(YanDialog* d, int button) override;
    
    void deleteKeyPressed(int lastRowSelected) override;
    void returnKeyPressed(int lastRowSelected) override;
    
  private:
    
    class BindingEditor* editor = nullptr;
    class BindingSets* bindingSets = nullptr;
    juce::String objectTypeName;
    
    juce::OwnedArray<class BindingSetTableRow> bindingSetRows;
    
    YanPopup rowPopup {this};
    YanPopup emptyPopup {this};
    
    YanDialog nameDialog {this};
    YanDialog propertiesDialog {this};
    YanDialog deleteAlert {this};
    YanDialog confirmDialog {this};
    YanDialog errorAlert {this};
    
    YanInput newName {"New Name"};
    YanInput propName {"Name"};
    YanCheckbox propOverlay {"Overlay"};
    
    class BindingSet* getSelectedSet();
    
    void startNew();
    void startCopy();
    void startProperties();
    void startDelete();

    void finishNew(int button);
    void finishCopy(int button);
    void finishProperties(int button);
    void finishDelete(int button);

    void showResult(juce::StringArray& errors);

};
    
