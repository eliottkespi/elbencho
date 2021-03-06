#ifndef LOCALWORKER_H_
#define LOCALWORKER_H_

#include <ctype.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "CuFileHandleData.h"
#include "FileOffsetGenerator.h"
#include "Worker.h"

// delaration for function typedefs below
class LocalWorker;

// io_prep_pwrite or io_prep_read from libaio
typedef void (*AIO_RW_PREPPER)(struct iocb* iocb, int fd, void* buf, size_t count,
	long long offset);

// {pread,pwrite}Wrapper (as in "man 2 pread") or cuFile{Read,Write}Wrapper
typedef ssize_t (LocalWorker::*POSITIONAL_RW)(int fd, void* buf, size_t nbytes, off_t offset);

// function pointer for GPU memcpy before write or after read
typedef void (LocalWorker::*GPU_MEMCPY_RW)(size_t count);

// function pointer for sync or async IO
typedef ssize_t (LocalWorker::*RW_BLOCKSIZED)(int fd);

// function pointer for cuFile handle register
typedef void (LocalWorker::*CUFILE_HANDLE_REGISTER)(int fd, CuFileHandleData& handleData);

// function pointer for cuFile handle deregister
typedef void (LocalWorker::*CUFILE_HANDLE_DEREGISTER)(CuFileHandleData& handleData);


/**
 * Each worker represents a single thread performing local I/O.
 */
class LocalWorker : public Worker
{
	public:
		explicit LocalWorker(WorkersSharedData* workersSharedData, size_t workerRank) :
			Worker(workersSharedData, workerRank) {}

		~LocalWorker();


	private:
	    std::mt19937_64 randGen{std::random_device()() }; // random_device is just for random seed

		void* ioBuf{NULL}; // buffer used for block-sized read()/write()

		void* gpuIOBuf{NULL}; // gpu memory buffer for read/write via cuda
		int gpuID{-1}; // GPU ID for this worker, initialized in allocGPUIOBuffer
		CuFileHandleData cuFileHandleData; // cuFile handle for current file in dir mode
		CuFileHandleData* cuFileHandleDataPtr{NULL}; // for cuFileRead/WriteWrapper

		// phase-dependent function & offsetGen pointers
		RW_BLOCKSIZED funcRWBlockSized; // pointer to sync or async read/write
		POSITIONAL_RW funcPositionalRW; // pread/write, cuFileRead/Write for sync read/write
		AIO_RW_PREPPER funcAioRwPrepper; // io_prep_pwrite/io_prep_read for async read/write
		GPU_MEMCPY_RW funcPreWriteCudaMemcpy; // copy from GPU memory
		GPU_MEMCPY_RW funcPostReadCudaMemcpy; // copy to GPU memory
		CUFILE_HANDLE_REGISTER funcCuFileHandleReg; // cuFile handle register
		CUFILE_HANDLE_DEREGISTER funcCuFileHandleDereg; // cuFile handle deregister
		FileOffsetGenerator* offsetGen{NULL}; // offset generator for phase-dependent funcs

		virtual void run() override;

		void finishPhase();

		void getPhaseFileRange(uint64_t& outFileRangeStart, uint64_t& outFileRangeLen);
		void initPhaseOffsetGen();
		void initPhaseFunctionPointers();

		void allocIOBuffer();
		void allocGPUIOBuffer();

		ssize_t rwBlockSized(int fd);
		ssize_t aioBlockSized(int fd);

		void iterateDirs();
		void dirModeIterateFiles();
		void fileModeIterateFiles();

		int getDirModeOpenFlags(BenchPhase benchPhase);

		// for phase function pointers...

		void noOpCudaMemcpy(size_t count);
		void preWriteCudaMemcpy(size_t count);
		void postReadCudaMemcpy(size_t count);

		void noOpCuFileHandleReg(int fd, CuFileHandleData& handleData);
		void noOpCuFileHandleDereg(CuFileHandleData& handleData);
		void dirModeCuFileHandleReg(int fd, CuFileHandleData& handleData);
		void dirModeCuFileHandleDereg(CuFileHandleData& handleData);
		void fileModeCuFileHandleReg(int fd, CuFileHandleData& handleData);
		void fileModeCuFileHandleDereg(CuFileHandleData& handleData);

		ssize_t preadWrapper(int fd, void* buf, size_t nbytes, off_t offset);
		ssize_t pwriteWrapper(int fd, void* buf, size_t nbytes, off_t offset);
		ssize_t cuFileReadWrapper(int fd, void* buf, size_t nbytes, off_t offset);
		ssize_t cuFileWriteWrapper(int fd, void* buf, size_t nbytes, off_t offset);
};


#endif /* LOCALWORKER_H_ */
