
#include <JuceHeader.h>

#include "../ui/common/YanDialog.h"
#include "../mobius/TrackContent.h"

#include "Task.h"

class ProjectExportTask : public Task, public YanDialog::Listener
{
  public:
    
    ProjectExportTask();

    void run() override;

    void cancel();
    
    // internal use but need to be public for the async notification?
    // void continueWorkflow(bool cancel);
    
    void yanDialogClosed(YanDialog* d, int button);

  private:

    juce::File projectContainer;
    juce::File projectFolder;

    std::unique_ptr<class TrackContent> content;

    YanDialog confirmDialog {this};
    YanDialog resultDialog {this};
    
    bool warnOverwrite = false;

    // workflow steps
    void confirmDestination();
    void locateProjectDestination();
    juce::File generateUnique(juce::String desired);
    void chooseDestination();
    void showResult();

    // final file creation
    void compileProject();

};


