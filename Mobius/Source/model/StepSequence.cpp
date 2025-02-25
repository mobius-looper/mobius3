
#include <string>

#include "../util/Util.h"

#include "StepSequence.h"

StepSequence::StepSequence()
{
	memset(mSource, 0, sizeof(mSource));
	mStepCount = 0;
}

StepSequence::StepSequence(const char* src)
{
	mStepCount = 0;
	setSource(src);
}

StepSequence::~StepSequence()
{
}

void StepSequence::reset()
{
	setSource(nullptr);
}

void StepSequence::setSource(const char* src) 
{
	if (src == nullptr)
	  memset(mSource, 0, sizeof(mSource));
	else {
		// bounds check!
		strcpy(mSource, src);
	}

	for (int i = 0 ; i < MAX_SEQUENCE_STEPS ; i++) 
	  mSteps[i] = 0;

	mStepCount = ParseNumberString(src, mSteps, MAX_SEQUENCE_STEPS);

	// TODO: could normalize this way?
#if 0
	char normalized[1024];
	strcpy(normalized, "");
	for (int i = 0 ; i < mStepCount ; i++) {
		char num[32];
		snprintf(num, sizeof(num), "%d", mSteps[i]);
		if (i > 0)
		  strcat(normalized, " ");
		strcat(normalized, num);
	}
	strcpy(mSource, normalized);
#endif
}

const char* StepSequence::getSource()
{
	return mSource;
}

int* StepSequence::getSteps()
{
	return mSteps;
}

int StepSequence::getStepCount()
{
	return mStepCount;
}

int StepSequence::advance(int current, bool next, int dflt,
						  int* retval)
{
	int value = dflt;
	int index = current;

	if (mStepCount > 0) {
		if (next) {
			index++;
			if (index >= mStepCount)
			  index = 0;
		}
		else {
			index--;
			if (index < 0)
			  index = mStepCount  - 1;
		}
		value = mSteps[index];
	}

	if (retval)
	  *retval = value;

	return index;
}

/**
 * Necessary to copy the "real" Preset sequence into the one that
 * Track and Loop own.
 */
void StepSequence::copy(StepSequence* src) 
{
	strcpy(mSource, src->mSource);
	mStepCount = src->mStepCount;
	for (int i = 0 ; i < mStepCount ; i++) 
	  mSteps[i] = src->mSteps[i];
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
