/*
 * Experimental tool to direct Trace messages to a log file.
 * Hacked together when TraceClient/DebugWindow was broken.
 *
 * Not terribly concerned with efficiency here, just get something
 * functional for the unit tests.
 *
 */

class TraceFile
{
  public:

    TraceFile();
    ~TraceFile();

    void setEnabled(bool b);

    void enable() {
        setEnabled(true);
    }
    void disable() {
        setEnabled(false);
    }
    
    bool setFile(juce::File file);
    void add(const char* msg);
    void flush();
    void clear();
    
  private:
    
    bool enabled = false;
    
    int lines = 0;
    int linesPerFlush = 10;
    juce::String buffer;
    juce::File logfile;
    
};

extern TraceFile TraceFile;
