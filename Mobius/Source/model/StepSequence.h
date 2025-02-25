/**
 * Represents a sequence of "steps" which are integers.
 * Used for both rate and pitch sequences.
 * Factored out of the Preset which is going away.
 *
 * This isn't part of the persistent model, it is built at runtime
 * From a delimited string to make it easure to deal with.
 *
 * This is very old and could use a good redesign.
 */

#pragma once

#include "ParameterConstants.h"

/**
 * Represents a sequence of "steps" which are integers.
 * Used for both rate and pitch sequences.
 */
class StepSequence {
  public:

	StepSequence();
	StepSequence(const char* source);
	~StepSequence();

	void reset();
	void copy(StepSequence* src);

	void setSource(const char* src);
	const char* getSource();
	int* getSteps();
	int getStepCount();

	int advance(int current, bool next, int dflt, int* retval);

  private:

	/**
	 * The text representation of the rate sequence.
	 * This should be a list of numbers delimited by spaces.
	 */
	char mSource[MAX_SEQUENCE_SOURCE];

	/**
	 * A sequence of rate transposition numbers, e.g. -1, 7, -5, 3
	 * compiled from mSource.  The mStepCount field
	 * has the number of valid entries.
	 */
	int mSteps[MAX_SEQUENCE_STEPS];

	/**
	 * Number of compiled rate sequence steps in mSteps;
	 */
	int mStepCount;
};

