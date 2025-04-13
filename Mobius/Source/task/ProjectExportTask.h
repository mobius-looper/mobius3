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
        ChooseContainer,
        ChooseFolder,
        Export,
        Results,
        Cancel
    } Step;
    
    ProjectExportTask();

    void run() override;
    void cancel();
    
    // internal use but need to be public for the async notification?
    void yanDialogClosed(YanDialog* d, int button);

  private:

    juce::File projectContainer;
    juce::File projectFolder;

    TaskPromptDialog dialog {this};
    std::unique_ptr<juce::FileChooser> chooser;

    Step step = Start;

    std::unique_ptr<class TrackContent> content;

    // workflow steps
    void locateProjectDestination();
    juce::File generateUnique(juce::String desired);
    void confirmDestination();
    void chooseDestination();
    void finishChooseDestination(juce::File choice);

    void doExport();
    void compileProject();
    void showResult();

};


