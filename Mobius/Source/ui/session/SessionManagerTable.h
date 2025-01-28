/**
 * A table showing available sessions.
 */

#pragma once

#include <JuceHeader.h>

#include "../script/TypicalTable.h"
#include "../common/YanPopup.h"
#include "../common/YanDialog.h"

#include "../../Producer.h"

class SessionManagerTableRow
{
  public:
    SessionManagerTableRow() {
    }
    ~SessionManagerTableRow() {
    }

    juce::String name;
    
};

class SessionManagerTable : public TypicalTable, public YanPopup::Listener,
                            public YanDialog::Listener
{
  public:

    const int ColumnName = 1;

    typedef enum {
        DialogLoad = 1,
        DialogCopy,
        DialogNew,
        DialogRename,
        DialogDelete
    } Dialog;
    
    SessionManagerTable(class Supervisor* s);
    ~SessionManagerTable();

    void load();
    void clear();

    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event);

    void mouseDown(const juce::MouseEvent& event);
    
    void yanPopupSelected(YanPopup* src, int id);
    void yanDialogClosed(YanDialog* d, int button);
    
  private:
    
    class Supervisor* supervisor = nullptr;
    class Producer* producer = nullptr;
    
    juce::OwnedArray<class SessionManagerTableRow> sessions;
    juce::StringArray names;
    
    YanPopup rowPopup {this};
    YanPopup emptyPopup {this};
    
    YanDialog nameDialog {this};
    YanDialog deleteAlert {this};
    YanDialog confirmDialog {this};
    YanDialog errorAlert {this};
    
    YanInput newName {"New Name"};
    
    void reload();
    juce::String getSelectedName();
    
    void startLoad();
    void startNew();
    void startCopy();
    void startRename();
    void startDelete();

    void finishLoad(int button);
    void finishNew(int button);
    void finishCopy(int button);
    void finishRename(int button);
    void finishDelete(int button);
    
    void showResult(Producer::Result& result);
};
    
