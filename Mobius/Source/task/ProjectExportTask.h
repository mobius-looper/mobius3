/**
 *  Workflow for project export
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
        Start,
        WarnMissingUserFolder,
        WarnInvalidUserFolder,
        ChooseContainer,
        ChooseSnapshot,
        VerifyFolder,
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
    juce::File projectContainer;
    juce::File projectFolder;

    TaskPromptDialog dialog {this};
    std::unique_ptr<juce::FileChooser> chooser;

    YanInput snapshotName {"Snapshot Name"};
    SnapshotChooser snapshotChooser;
    
    Step step = Start;

    std::unique_ptr<class TrackContent> content;

    // workflow steps

    void transition();
    void fileChosen(juce::File file);
    void findContainer();
    void warnMissingUserFolder();
    void warnInvalidUserFolder();
    void chooseContainer();
    void chooseSnapshot();
    void verifyFolder();
    bool isEmpty(juce::File f);
    void warnOverwrite();
    void doExport();
    void showResult();

};


