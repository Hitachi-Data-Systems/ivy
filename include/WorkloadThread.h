//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <list>
#include <stack>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <string>

enum class ThreadState
{
	waiting_for_command,
	running,
	stopping, // for example catching ing-flight I/Os after final subinterval
	died,
	exited_normally
};

std::string threadStateToString (ThreadState);

enum class MainThreadCommand
{
	run, stop, die
};

std::string mainThreadCommandToString(MainThreadCommand);


class WorkloadThread {
public:

// variables
	WorkloadID workloadID; // to get the string form of the workload ID, refer to workloadID.workloadID.
	LUN* pLUN;
	long long int maxLBA;
	iogenerator_stuff iogenerator_variables;
	std::string iogeneratorParameters; // just used to print a descriptive line when the thread fires up.
	std::mutex* p_ivyslave_main_mutex;

	std::thread std_thread;


	std::mutex slaveThreadMutex;
	std::condition_variable slaveThreadConditionVariable;


	ivytime nextIO_schedule_time;  // 0 means means right away.  (This is initialized later.)
	ivytime now;
	ivytime subinterval_duration, currentSubintervalEndTime;
	ivytime waitduration;

	Subinterval subinterval_array[2];

	Subinterval* p_current_subinterval;
	Subinterval* p_other_subinterval;

	IogeneratorInput* p_current_IogeneratorInput;
	IogeneratorInput* p_other_IogeneratorInput;
	SubintervalOutput* p_current_SubintervalOutput;
	SubintervalOutput* p_other_SubintervalOutput;

	Iogenerator* p_my_iogenerator;

	std::list<Eyeo*> allEyeosThatExist;

	std::stack<Eyeo*> freeStack;
		// we should come back later when this code runs multiple times and make sure
		// we keep a copy of the pointers to all the Eyeos we created.
		// this way we could just discard all the pointer structures / queues at the end of a test
		// and reinitialize them from scratch, reusing the same Eyeos to populate the freeStack at the beginning


	std::list<Eyeo*> precomputeQ;  // We are using a std::list instead of a std::queue so that
	                               // if io_submit() doesn't successfully launch all of a batch
	                               // then we can put the ones that didn't launch back into
	                               // the precomputeQ in original sequence at the wrong end backwards.
	//struct procstatcounters psc_start, psc_end;

	std::queue<Eyeo*> postprocessQ;

	Eyeo* LaunchPad[MAX_IOS_LAUNCH_AT_ONCE];

	struct io_event ReapHeap[MAX_IOEVENTS_REAP_AT_ONCE];

	std::map<std::string, Iogenerator*> available_iogenerators;

	bool dieImmediately{false};
	bool ivyslave_main_posted_command = false;  // This is used for thread interlock, so made it volatile just in case.


	MainThreadCommand ivyslave_main_says {MainThreadCommand::die};

	ThreadState state {ThreadState::died};

	int EyeoCount;
	int currentSubintervalIndex, otherSubintervalIndex, subinterval_number;  // The master thread never sets these or refers to them.
	int ioerrorcount=0;
	unsigned int precomputedepth;  // will be set to be equal to the maxTags spec when we get "Go!"
	int rc;
	unsigned int queue_depth{0};
	int events_needed;
	int reaped;
	unsigned int maxqueuedepth{0};

	std::string hostname;
	std::string slavethreadlogfile;

	uint64_t xorshift_s[16];
    int xorshift_p;

//methods
	WorkloadThread(std::string /*workloadID*/, LUN*, long long int /*lastLBA*/, std::string /*parms*/, std::mutex*);
	inline ~WorkloadThread() {};

    uint64_t xorshift1024star();

	std::string stateToString();
	std::string ivyslave_main_saysToString();
	void popandpostprocessoneEyeo();
	void WorkloadThreadRun();
	bool prepare_linux_AIO_driver_to_start();  // true means success. false means an error message has been logged and to abort.
	bool linux_AIO_driver_run_subinterval();
	bool catch_in_flight_IOs_after_last_subinterval();
		// Includes doing any necessary "turn around" things
		// to make sure we are ready to run with parameters
		// the same as the last one from the last run.

		// !!!!!!!!!!!!!!!!!!!  In future, consider waiting for Write Pending to empty in the "catch ..." routine.

};

