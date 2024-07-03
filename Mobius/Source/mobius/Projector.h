/**
 * Utility to read and write project files.
 * This used to be intertwined with core/Project.cpp but split
 * out so we can keep file handling above the kernel.
 */

#pragma once

class Projector
{
  public:
    Projector();
    ~Projector();

    // callers must check isError 
    void read();
	void read(class AudioPool* pool);
    void write();
	void write(const char* file, bool isTemplate);

    
    void deleteAudioFiles();

  private:
    
    void read(class AudioPool* pool, const char* file);
	void readAudio(class AudioPool* pool);
	void writeAudio(const char* baseName);

	//
	// Transient parse state
	//

	FILE* mFile;
	char mBuffer[1024];
	char** mTokens;
	int mNumTokens;

	//
	// Transient save state

	/**
	 * Set by the interrupt handler when the state of the project
	 * has been captured.
	 */
	bool mFinished;

};

    
