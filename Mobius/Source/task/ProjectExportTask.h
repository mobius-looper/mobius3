/**
 *  Workflow for project export
 */

#include <JuceHeader.h>

#include "../ui/common/YanDialog.h"
#include "../mobius/TrackContent.h"

#include "TaskPromptDialog.h"
#include "Task.h"

class ProjectExportTask : public Task, public YanDialog::Listener
{
  public:

    typedef enum {
        Start,
        WarnMissingUserFolder,
        WarnInvalidUserFolder,
        ChooseFolder,
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

    Step step = Start;

    std::unique_ptr<class TrackContent> content;

    // workflow steps

    void transition();
    void fileChosen(juce::File file);
    void findProjectContainer();
    void warnMissingUserFolder();
    void warnInvalidUserFolder();
    void chooseFolder();
    void verifyFolder();
    bool isEmpty(juce::File f);
    void warnOverwrite();
    void doExport();
    void showResult();

};


