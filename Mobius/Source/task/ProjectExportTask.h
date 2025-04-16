/**
 *  Workflow for snapshot export
 */

#include <JuceHeader.h>

#include "../ui/common/YanDialog.h"
#include "../ui/common/YanField.h"
#include "../ui/common/YanListBox.h"
#include "../mobius/TrackContent.h"

#include "TaskPromptDialog.h"
#include "Task.h"

class ProjectExportTask : public Task, public YanDialog::Listener
{
  public:

    class SnapshotChooser : public juce::Component {
      public:
        SnapshotChooser();
        YanInput snapshotName {"Snapshot Name"};
        YanListBox snapshots {"Existing Snapshots"};

        void resized() override;
    };

    typedef enum {
        FindContainer,
        WarnMissingUserFolder,
        WarnInvalidUserFolder,
        ChooseFolder,
        VerifyFolder,
        InvalidFolder,
        WarnOverwrite,
        Export,
        Result,
        Cancel
    } Step;
    
    ProjectExportTask();

    void run() override;
    void ping() override;
    void cancel() override;
    
    // internal use but need to be public for the async notification?
    void yanDialogClosed(YanDialog* d, int button) override;

  private:

    juce::String userFolder;
    juce::File snapshotContainer;
    juce::File snapshotFolder;

    TaskPromptDialog dialog {this};
    std::unique_ptr<juce::FileChooser> chooser;

    YanInput snapshotName {"Snapshot Name"};
    SnapshotChooser snapshotChooser;
    
    Step step = FindContainer;

    std::unique_ptr<class TrackContent> content;

    // workflow steps

    void transition();
    void fileChosen(juce::File file);
    
    void findContainer();
    void warnMissingUserFolder();
    void warnInvalidUserFolder();
    void chooseFolder();
    void verifyFolder();
    bool isEmpty(juce::File f);
    void invalidFolder();
    void warnOverwrite();
    void doExport();
    void showResult();
    
};


