/**
 * A table showing Bindings in a BindingSet.
 */

#pragma once

#include <JuceHeader.h>

#include "../script/TypicalTable.h"
#include "../common/YanPopup.h"
#include "../common/YanDialog.h"

#include "../../Producer.h"

class BindingTableRow
{
  public:
    BindingTableRow() {
    }
    ~BindingTableRow() {
    }

    class Binding* binding = nullptr;
    
};

class BindingTable : public TypicalTable,
                        public YanPopup::Listener,
                        public YanDialog::Listener
{
  public:

    typedef enum {
        TypeMidi,
        TypeKey,
        TypeHost
    } Type;

    // column ids 
    static const int TargetColumn = 1;
    static const int TriggerColumn = 2;
    static const int ArgumentsColumn = 3;
    static const int ScopeColumn = 4;
    static const int DisplayNameColumn = 5;

    // these are used as popup menu item ids so must start with zero
    typedef enum {
        DialogEdit = 1,
        DialogDelete,
        DialogHelp
    } Dialog;
    
    BindingTable();
    ~BindingTable();

    void load(class BindingEditor* e, class BindingSet* set, Type type);
    void reload();
    void clear();
    void cancel();

    void add(Binding* b);
                
    // TypicalTable overrides
    int getRowCount() override;
    juce::String getCellText(int rowNumber, int columnId) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

    void mouseDown(const juce::MouseEvent& event) override;
    
    void yanPopupSelected(YanPopup* src, int id) override;
    void yanDialogClosed(YanDialog* d, int button) override;
    
  private:

    class BindingEditor* editor = nullptr;
    class BindingSet* bindingSet = nullptr;
    Type type = TypeMidi;
    
    juce::OwnedArray<class BindingTableRow> bindingRows;
    
    YanPopup rowPopup {this}; 
    YanPopup emptyPopup {this};
    YanDialog helpDialog {this};
    
    void startHelp();

    void addBinding(class Binding* b);

};
    
