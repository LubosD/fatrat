#ifndef _RETRYPOLICY_H
#define _RETRYPOLICY_H

namespace RetryPolicy
{
	// Settings -> Queues: "Retry N times" and "Always retry if the transfer has been working before".
	// retriesDone is how many times the transfer has already been retried.
	inline bool shouldRetry(bool alwaysIfWorking, bool hasBeenWorking, int maxRetries, int retriesDone)
	{
		if(alwaysIfWorking && hasBeenWorking)
			return true;
		return maxRetries > retriesDone;
	}
}

#endif
