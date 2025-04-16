/**
 * Some adjustments from 2.x
 *
 * Immediately after load or GlobalRfeset we're effectively at the first index of 0
 * and if you then do Speednext it returns the second item in the list.
 *
 * It makes more sense to have index zero implicitly mean "none" and the first Next goes
 * to the first step in the list. Then when you advance past the end and wrap back to zero
 * there is an implicit zero step at the beginning.
 *
 * Easiest way to handle that is to inject a zero at the beginning during parsing
 * rather than making all the callers use -1 as an index and deal with the wrap.
 * 
 *
 */

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

/**
 * Hate the old-school interface here, but it's been working
 * so keep it going.
 *
 * This preteds there is always a zero at the front so the number
 * of steps is 1+ what was parsed.
 */
int StepSequence::advance(int current, bool next, int dflt,
						  int* retval)
{
    // with the leading zero, the deftult will never be used now
	int value = dflt;
	int index = current;
    // leading zero
    int max = mStepCount + 1;
    
	if (max > 0) {
		if (next) {
			index++;
			if (index >= max)
			  index = 0;
		}
		else {
			index--;
			if (index < 0)
			  index = max  - 1;
		}

        if (index == 0)
          value = 0;
        else
          value = mSteps[index - 1];
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
