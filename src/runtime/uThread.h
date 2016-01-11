/*
 * uThread.h
 *
 *  Created on: Oct 23, 2014
 *      Author:  Saman Barghi
 */
#pragma once
#include <mutex>
#include <atomic>
#include "generic/basics.h"
#include "generic/IntrusiveContainers.h"
#include "Stack.h"

class BlockingQueue;
class Mutex;
class Cluster;
class IOHandler;

//Thread states
enum uThreadStatus {
    INITIALIZED,                                                    //uThread is initialized
    READY,                                                          //uThread is in a ReadyQueue
    RUNNING,                                                        //uThread is Running
    YIELD,                                                          //uThread Yields
    MIGRATE,                                                        //Migrate to another cluster
    WAITING,                                                        //uThread is in waiting mode
    IOBLOCK,                                                        //uThread is blocked on IO
    TERMINATED                                                      //uThread is done and should be terminated
};


class uThread : public IntrusiveList<uThread>::Link{
	friend class kThread;
	friend class Cluster;
	friend class LibInitializer;
	friend class BlockingQueue;
	friend class IOHandler;
private:

	//TODO: Add a function to check uThread's stack for overflow ! Prevent overflow or throw an exception or error?
	//TODO: Fix uThread.h includes: if this is the file that is being included to use the library, it should include at least kThread and Cluster headers
	//TODO: Add a debug object to project, or a dtrace or lttng functionality
	//TODO: Check all functions and add assertions wherever it is necessary


	uThread(Cluster&);                                      //This will be called by default uThread
	uThread(const Cluster&, funcvoid1_t, ptr_t, ptr_t, ptr_t);                  //To create a new uThread, create function should be called

	static uThread	initUT;            //initial uT that is associated with main
	static uThread  ioUT;              //default IO uThread

	/*
	 * Statistics variables
	 */
	//TODO: Add more variables, number of suspended, number of running ...
	static std::atomic_ulong totalNumberofUTs;			//Total number of existing uThreads
	static std::atomic_ulong uThreadMasterID;			//The main ID counter
	uint64_t uThreadID;							//unique Id for this uthread

	/*
	 * Thread variables
	 */
	uThreadStatus status;				//Current status of the uThread, should be private only friend classes can change this
	Cluster*	currentCluster;			//This will be used for migrating to a new Cluster

	/*
	 * Stack Boundary
	 */
	size_t		stackSize;
	vaddr 		stackPointer;			// holds stack pointer while thread inactive
	vaddr		stackTop;				//Top of the stack
	vaddr		stackBottom;			//Bottom of the stack

	/*
	 * general functions
	 */
	vaddr createStack(size_t);			//Create a stack with given size
	void terminate();
	void suspend(std::function<void()>&);

	void initialSynchronization();		//Used for assigning a thread ID, set totalNumberofUTs and ...
	static void decrementTotalNumberofUTs();	//Decrement the number (only used in kThread with default uthread)

public:


	virtual ~uThread();                 //TODO: protected and nonvirtual? do we want to inherit from this ever?

	uThread(const uThread&) = delete;
	const uThread& operator=(const uThread&) = delete;

	const Cluster& getCurrentCluster() const;
	/*
	 * Thread management functions
	 */
	static uThread* create(const Cluster& cluster, funcvoid1_t func)                        {return create(cluster, func, nullptr, nullptr, nullptr);};
	static uThread* create(const Cluster& cluster, funcvoid1_t func, ptr_t arg1)            {return create(cluster, func, arg1, nullptr, nullptr);};
	static uThread* create(const Cluster& cluster, funcvoid1_t func, ptr_t arg1, ptr_t arg2){return create(cluster, func, arg1, arg2, nullptr);};
	static uThread* create(const Cluster& cluster, funcvoid1_t func, ptr_t arg1, ptr_t arg2, ptr_t arg3);

	static uThread* create(funcvoid1_t func)                                    {return create(func, nullptr, nullptr, nullptr);};
	static uThread* create(funcvoid1_t func, ptr_t arg1)                        {return create(func, arg1, nullptr, nullptr);};
	static uThread* create(funcvoid1_t func, ptr_t arg1, ptr_t arg2)            {return create(func, arg1, arg2, nullptr);};
	static uThread* create(funcvoid1_t func, ptr_t arg1, ptr_t arg2, ptr_t arg3);


	static void yield();
	void migrate(Cluster*);				//Migrate the thread to a new Cluster
	void resume();
	static void uexit();				//End the thread

	/*
	 * general functions
	 */
	static uint64_t getTotalNumberofUTs();
	uint64_t getUthreadId() const;
};
