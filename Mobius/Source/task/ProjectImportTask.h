
#include <JuceHeader.h>

#include "../ui/common/YanDialog.h"
#include "../mobius/TrackContent.h"

#include "Task.h"
#include "TaskPromptDialog.h"

class ProjectImportTask : public Task, public YanDialog::Listener
{
  public:
    
    typedef enum {
        Start,
        Inspect,
        MismatchedTracks,
        ImportNew,
        ImportOld,
        Result,
        Cancel
    } Step;
    
    ProjectImportTask();

    void run() override;
    void ping() override;
    void cancel() override;
    
    void yanDialogClosed(YanDialog* d, int button) override;
    void fileChosen(juce::File file);
    
  private:

    Step step = Start;

    juce::File importFile;
    std::unique_ptr<class TrackContent> content;
    
    TaskPromptDialog dialog {this};
    std::unique_ptr<juce::FileChooser> chooser;

    void transition();
    void findImport();
    void inspectImport();
    void doImportNew();
    void doImportOld();
    void showResult();

};


