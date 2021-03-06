#include "NumaTk.h"
#include "Worker.h"
#include "WorkerException.h"


/**
 * Instantiate a worker object and call its run method.
 */
void Worker::threadStart(Worker* worker)
{
	worker->run();
}


/**
 * Increase number of done and workers and create stonewall stats if this is the first worker to
 * finish.
 */
void Worker::incNumWorkersDone()
{
	std::unique_lock<std::mutex> lock(workersSharedData->mutex); // L O C K (scoped)

	workersSharedData->incNumWorkersDoneUnlocked();

	/* create stonewall stats when 1st worker finishes (mutex guarantees that no other worker
	   increases the done counter in the meantime) */
	if( (workersSharedData->numWorkersDone == 1) && !stoneWallTriggered)
	{
		for(Worker* worker : *workersSharedData->workerVec)
			worker->createStoneWallStats();
	}
}

/**
 * Increase number of done and workers with error after the current phase has finished or been
 * cancelled with an error.
 */
void Worker::incNumWorkersDoneWithError()
{
	ErrLogger(Log_DEBUG) << "Increasing done with error counter. " <<
		"WorkerRank: " << this->workerRank << std::endl;

	workersSharedData->incNumWorkersDoneWithError();
}

/**
 * Check if this worker has been friendly asked to interrupt itself.
 *
 * @throw WorkerInterruptedException if friendly ask to interrupt has been received.
 */
void Worker::checkInterruptionRequest()
{
	IF_UNLIKELY(isInterruptionRequested)
		throw WorkerInterruptedException("Received friendly request to interrupt execution.");
}

/**
 * Apply configured NUMA binding to calling worker thread. Do nothing if NUMA binding is not
 * configured.
 *
 * @throw WorkerException on error, e.g. if binding fails.
 */
void Worker::applyNumaBinding()
{
	const IntVec& numaZonesVec = progArgs->getNumaZonesVec();

	if(numaZonesVec.empty() || !NumaTk::isNumaInfoAvailable() )
		return;

	int zoneNum = numaZonesVec[workerRank % numaZonesVec.size() ];

	try
	{
		NumaTk::bindToNumaZone(std::to_string(zoneNum) );
	}
	catch(ProgException& e)
	{
		// turn NumaTk's ProgException into WorkerException
		throw WorkerException(e.what() );
	}

}

/**
 * Wait for notification to start with next phase.
 *
 * @oldBenchID wait for workersSharedData->currentBenchID to be different from this value.
 */
void Worker::waitForNextPhase(const buuids::uuid& oldBenchID)
{
	std::unique_lock<std::mutex> lock(workersSharedData->mutex); // L O C K (scoped)

	checkInterruptionRequest();

	while(oldBenchID == workersSharedData->currentBenchID)
	{
		workersSharedData->condition.wait(lock);
		checkInterruptionRequest();
	}
}


