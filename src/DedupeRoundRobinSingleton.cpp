//Copyright (c) 2020 Hitachi Vantara Corporation
//All Rights Reserved.
//
//   Licensed under the Apache License, Version 2.0 (the "License"); you may
//   not use this file except in compliance with the License. You may obtain
//   a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
//   License for the specific language governing permissions and limitations
//   under the License.
//
//Authors: Stephen Morgan <stephen.morgan@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include <stdlib.h>

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "ivydefines.h"
#include "ivydriver.h"
#include "DedupeRoundRobinSingleton.h"
#include "Workload.h"

void DedupeRoundRobinSingleton::init(MyWorkloadMap::iterator itr)
{
	struct workload_state *wsp = itr->second;
	if (debugging) {
		assert(wsp != nullptr);
	}

    wsp->LUN_count = 0;
    wsp->LUN_max_plus_one = 0;
    wsp->iosize = 0;
    wsp->dedupe_blocks = 0;
    wsp->dedupe_unit_bytes = 0;
    wsp->duplicate_set_size = 0;
    wsp->duplicates = 0;
    wsp->uniques = 0;
    wsp->init_duplic_seed = 0x0123456789abcdefULL;
    wsp->init_unique_seed = 0xfedcba9876543210ULL;
    wsp->curr_unique_seed = 0;
    if (wsp->duplicate_set != nullptr) {
    	delete wsp->duplicate_set;
        wsp->duplicate_set = nullptr;
    }
    wsp->dedupe_ratio = 0.0F;
    wsp->correction_factor = 0.0F;
    wsp->LUN_assigned.clear();
    wsp->LUN_to_LDEV.clear();
    wsp->LUN_to_index.clear();
    wsp->hash_to_LUN.clear();
    wsp->initialized = true;
}

void DedupeRoundRobinSingleton::compute_factors(MyWorkloadMap::iterator itr)
{
//	get_nsecs();	// For performance tuning.

	struct workload_state *wsp = itr->second;
	if (debugging) {
		assert(wsp != nullptr);
		assert(wsp->initialized);
	}

	// Correct for impact of duplicate set on deduplication ratio.

    wsp->correction_factor =
    		((ivy_float) wsp->dedupe_blocks / wsp->dedupe_ratio + (ivy_float) wsp->duplicate_set_size) /
								((ivy_float) wsp->dedupe_blocks / wsp->dedupe_ratio);

    // Extract those parts of the corrected deduplication ratio both before and after the decimal point.

    uint64_t  rational_fixed = (uint64_t) (wsp->dedupe_ratio * wsp->correction_factor / 1);
    ivy_float rational_float = (wsp->dedupe_ratio * wsp->correction_factor) - (ivy_float) (rational_fixed * 1.0);

    // Compute a rational approximation to the part of the corrected dedupe_ratio after the decimal point.
    // This call limits the error to 0.000001 and the number of duplicates to 1,000.

    pair<uint64_t, uint64_t> pp = farey(rational_float, 0.000001, 1000);

    // Determine the number of uniques and duplicates in each group.

    wsp->uniques    = pp.second;
    wsp->duplicates = pp.first + pp.second * (uint64_t) (wsp->dedupe_ratio * wsp->correction_factor - rational_float - 1.0);

//	uint64_t nsecs1 = get_nsecs();	// For performance tuning.

    // Generate the LUN_to_index map from the LUN_to_LDEV map. Works even if some or all LDEVs are zero (0).

    // First, invert the LUN_to_LDEV map to create an LDEV_to_LUN map.
    map<unsigned, int> LDEV_to_LUN;
	for (int i = 0; i < wsp->LUN_max_plus_one; i++) {
		if (wsp->LUN_assigned[i]) {
			LDEV_to_LUN[wsp->LUN_to_LDEV[i]] = i;
		}
	}
	// Next, clear the LUN_to_index map.
	wsp->LUN_to_index.clear();
	// Finally, create the LUN_to_index map from the LDEV_to_LUN map.
	for (int i = 0; i < wsp->LUN_max_plus_one; i++) {
		auto itr1 = LDEV_to_LUN.begin();
		if (wsp->LUN_assigned[i]) {
			auto itr2 = LDEV_to_LUN.find(wsp->LUN_to_LDEV[i]);
			wsp->LUN_to_index[i] = distance(itr1, itr2);
		}
	}

//	uint64_t nsecs2 = get_nsecs();	// For performance tuning.

//	if (logging)
//	{
//		ostringstream oss;
//		oss << __func__ << "(" << __LINE__ << "): "
//			<< "comp ns = " << nsecs1
//			<< ", sort ns = " << nsecs2
//			<< "." << endl;
//		logger logfile {"/tmp/spm.txt"};
//		log(logfile, oss.str());
//
//		for (int i = 0; i < wsp->LUN_max_plus_one; i++) {
//			if (wsp->LUN_assigned[i]) {
//				ostringstream oss;
//				oss << __func__ << "(" << __LINE__ << "): "
//					<< "LUN_to_index[" << setw(4) << i << "] = " << setw(4) << wsp->LUN_to_index[i]
//                       << " (LDEV = 0x" << hex << setw(8) << setfill('0')
//					   << wsp->LUN_to_LDEV[i] << dec << setfill(' ') << ")"
//					<< "." << endl;
//				logger logfile {"/tmp/spm.txt"};
//				log(logfile, oss.str());
//			}
//		}
//	}
}

void DedupeRoundRobinSingleton::add_lun(string &host_part, string &lun_part, string &work_part,
									 	uint64_t host_hash, uint64_t lun_hash, uint64_t work_hash,
										uint64_t my_blocks, uint64_t my_blocksize,
										ivy_float my_dedupe_ratio, uint64_t my_dedupe_unit_bytes,
										LUN *pLUN)
{
	mut.lock();

//	if (logging)
//	{
//		ostringstream oss;
//		oss << __func__ << "(" << __LINE__ << "): "
//			<< "work_part = \"" << work_part << "\""
//			<< ", lun_part = \"" << lun_part << "\""
//			<< ", my_blocks = " << my_blocks
//			<< ", my_blocksize = " << my_blocksize
//			<< ", my_dedupe_ratio = " << my_dedupe_ratio
//			<< ", my_dedupe_unt_bytes = " << my_dedupe_unit_bytes
//			<< "." << endl;
//		logger logfile {"/tmp/spm.txt"};
//		log(logfile, oss.str());
//	}

	if (debugging) {
		assert(my_blocks > 0);
		assert(my_blocksize > 0);
		assert(my_dedupe_ratio >= 1.0);
		assert(my_dedupe_unit_bytes > 0);
		assert(my_dedupe_unit_bytes % dedupe_unit_bytes_default == 0);
		assert (pLUN != nullptr);
	}

	// Find the workload_map structure and workload state (if any) associated with this workload.

	MyWorkloadMap::iterator itr = workload_map.find(work_hash);
	if (itr == workload_map.end()) {

		// If the structure does not already exist, create one for it.

		(void) workload_map[work_hash];		// Create empty workload map entry.
		itr = workload_map.find(work_hash);	// Find the new map entry.
		if (debugging) {
			assert(itr != workload_map.end());
		}
		itr->second = new (nothrow) struct workload_state;	// Allocate workload state structure.
		if (debugging) {
			assert(itr->second != nullptr);
		}
		(itr->second)->duplicate_set = nullptr;		// Ensure duplicate set is empty.
		(itr->second)->initialized = false;			// Mark the workload state structure as uninitialized.
	}

	// Get a pointer to the workload state structure.

	struct workload_state *wsp = itr->second;
	if (debugging) {
		assert(wsp != nullptr);
	}

	// Initialize workload state if necessary.

	if (! wsp->initialized) {
		init(itr);
	}

	// Don't know what to do if the key parameters in this coverage differ from that of other LUNs.

	wsp->dedupe_blocks += my_blocks * my_blocksize / my_dedupe_unit_bytes;
	wsp->work_part = work_part;
	wsp->iosize = my_blocksize;
	wsp->dedupe_ratio = my_dedupe_ratio;
	wsp->dedupe_unit_bytes = my_dedupe_unit_bytes;

//	if (logging)
//	{
//		ostringstream oss;
//		oss << __func__ << "(" << __LINE__ << "): "
//			<< "work_part = \"" << work_part << "\""
//			<< ", lun_part = \"" << lun_part << "\""
//			<< ", iosize = " << wsp->iosize
//			<< ", dedupe_ratio = " << wsp->dedupe_ratio
//			<< ", dedupe_unit_bytes = " << wsp->dedupe_unit_bytes
//			<< ", initialized = " << wsp->initialized
//			<< "." << endl;
//		logger logfile {"/tmp/spm.txt"};
//		log(logfile, oss.str());
//	}

	// Keep updating the duplicate set size until the duplicate set has been allocated and initialized.

	if (wsp->duplicate_set == nullptr) {
		wsp->duplicate_set_size = (uint64_t) (wsp->dedupe_blocks * duplicate_set_size_ratio);
	}

	// Remember the number of LUNs in this workload as well as this LUN's index.
	// Re-use an unassigned LUN if possible.

	uint64_t LUN;

	bool found = false;
	auto LUN_itr = wsp->hash_to_LUN.find(lun_hash);
	if (LUN_itr != wsp->hash_to_LUN.end()) {
		found = true;
		LUN = LUN_itr->second;
		wsp->LUN_count++;
	}
	if (! found) {
		for (int i = 0; i < wsp->LUN_max_plus_one; i++) {
			if (! wsp->LUN_assigned[i]) {
				found = true;
				LUN = i;
				wsp->LUN_count++;
				break;
			}
		}
	}
	if (! found) {
		LUN = wsp->LUN_count++;
		wsp->LUN_max_plus_one++;
	}

	wsp->LUN_assigned[LUN] = true;
	wsp->hash_to_LUN[lun_hash] = LUN;

	// Find the LDEV (if any) associated with this LUN.
	// Note: There will only be an LDEV if there is a command device connector.

	string ldev = pLUN->attribute_value(string("ldev"));
	trim(ldev);
	toUpper(ldev);
	unsigned LDEV = get_ldev(ldev); // Will return 0 if ldev is empty or malformed string.
	wsp->LUN_to_LDEV[LUN] = LDEV;

	// Don't know whether this is the last LUN for this workload. Assume not and compute
	// factors necessary to generate seeds. Will update later if additional LUN(s) added.

	compute_factors(itr);

//	if (logging)
//	{
//		ostringstream oss;
//		oss << __func__ << "(" << __LINE__ << "): "
//   			<< "work_part = \"" << work_part << "\""
//			<< ", lun_part = \"" << lun_part << "\""
//			<< ", LUN_count = " << wsp->LUN_count
//			<< ", LUN_max_plus_one = " << wsp->LUN_max_plus_one
//			<< ", LUN = " << LUN
//			<< ", LUN_assigned = " << wsp->LUN_assigned[LUN];
//		if 	(ldev.size() > 0) {
//			oss << ", ldev = \"" << ldev << "\""
//				<< ", LDEV = " << hex << showbase << internal << setfill('0') << setw(10) << LDEV << setfill(' ') << dec;
//		}
//		oss << ", blocks = " << my_blocks
//			<< ", iosize = " << wsp->iosize
//			<< ", dedupe_ratio = " << wsp->dedupe_ratio
//			<< ", dedupe_unit = " << wsp->dedupe_unit_bytes
//			<< ", dedupe_blocks = " << wsp->dedupe_blocks
//			<< ", dup_set_size = " << wsp->duplicate_set_size
//			<< ", dup_set_ptr = " << wsp->duplicate_set
//			<< ", correction_factor = " << wsp->correction_factor
//			<< ", uniques = " << wsp->uniques
//			<< ", duplicates = " << wsp->duplicates
//			<< ", act dedupe ratio = " << ((ivy_float) (wsp->uniques + wsp->duplicates) / wsp->uniques)
//			<< "." << endl;
//		logger logfile {"/tmp/spm.txt"};
//		log(logfile, oss.str());
//	}

	mut.unlock();
}

void DedupeRoundRobinSingleton::del_lun(string &host_part, string &lun_part, string &work_part,
										uint64_t host_hash, uint64_t lun_hash, uint64_t work_hash)
{
	mut.lock();

	// Find the workload_map structure (if any) associated with this workloadID.

	MyWorkloadMap::iterator itr = workload_map.find(work_hash);
	if (itr == workload_map.end()) {
		// Entire workload was already deleted.
		mut.unlock();
		return;
	}

	// Get a pointer to the workload state structure.

	struct workload_state *wsp = itr->second;
	if (debugging) {
		assert(wsp != nullptr);
		assert(wsp->initialized);
		assert(work_part.compare(wsp->work_part) == 0);	// Sanity check, to be removed eventually.
	}

	// Discard the specified LUN. If it was the final LUN, re-initialize the internal data fields.
	// (This acts like a destructor.)

	auto LUN_itr = wsp->hash_to_LUN.find(lun_hash);
	if (LUN_itr == wsp->hash_to_LUN.end()) {
		// The specified LUN was already deleted.
		mut.unlock();
		return;
	}

	// Found the specified LUN, so delete it.

	uint64_t LUN = LUN_itr->second;
	if (debugging) {
		assert(wsp->LUN_assigned[LUN]); // Sanity check.
	}
	wsp->LUN_assigned[LUN] = false;
	wsp->LUN_to_LDEV.erase(LUN);
	wsp->LUN_to_index.erase(LUN);
	wsp->hash_to_LUN.erase(lun_hash);
	wsp->LUN_count--;

	// Discard LUNs at end of list until we find an assigned LUN.

	if (wsp->LUN_max_plus_one > 0) {
		int decrement_count = 0;
		for (int i = wsp->LUN_max_plus_one - 1; i >= 0; i--) {
			if (wsp->LUN_assigned[i]) {
				break;
			}
			decrement_count++;;
		}
		wsp->LUN_max_plus_one -= decrement_count;
	}

	// If there are no more LUNs defined in this workload, remove the map entry.

	if (wsp->LUN_max_plus_one == 0) {
		workload_map.erase(itr);
	}

//	if (logging)
//	{
//		ostringstream oss;
//		oss << __func__ << "(" << __LINE__ << "): "
//			<< "work_part = \"" << work_part << "\""
//			<< ", lun_part = \"" << lun_part << "\""
//			<< ", LUN_count = " << wsp->LUN_count
//			<< ", LUN_max_plus_one = " << wsp->LUN_max_plus_one
//			<< ", LUN = " << LUN << " (deleted)"
//			<< "." << endl;
//		logger logfile {"/tmp/spm.txt"};
//		log(logfile, oss.str());
//	}

	mut.unlock();
}

uint64_t DedupeRoundRobinSingleton::get_seed(string &host_part, string &lun_part, string &work_part,
											 uint64_t host_hash, uint64_t lun_hash, uint64_t work_hash,
											 uint64_t LBA)
{
	mut.lock();

//	get_nsecs();	// For performance tuning.

	// Find the workload associated with this I/O request.
	// Note: I/O requests can arrive out-of-order with respect to del_LUN() requests.

	auto itr = workload_map.find(work_hash);
	if (itr == workload_map.end()) {
		mut.unlock();
		return 0;	// Generate a zero-filled block.
	}

	struct workload_state *wsp = itr->second;
	if ((wsp == nullptr) ||
		(! wsp->initialized)) {
		mut.unlock();
		return 0;	// Generate a zero-filled block.
	}

//	uint64_t nsecs1 = get_nsecs();	// For performance tuning.

	// If the duplicate set is not yet instantiated, instantiate and initialize it now.
	// This also initializes a bunch of other parameters, etc.

	if (wsp->duplicate_set == nullptr) {

		// If appropriate, allocate storage for the duplicate set.

		if (wsp->duplicate_set_size > 0) {
			wsp->duplicate_set = new (nothrow) uint64_t[wsp->duplicate_set_size];
		}

		// duplicate_set pointer could be equal to nullptr if new request failed.

        if (wsp->duplicate_set != nullptr) {

        	// Initialize the duplicate set from the initial duplicate seed.

        	wsp->duplicate_set[0] = wsp->init_duplic_seed;
        	xorshift64star(wsp->duplicate_set[0]);
        	for (int i = 1; i < wsp->duplicate_set_size; i++)
        	{
        		wsp->duplicate_set[i] = wsp->duplicate_set[i-1];
        		xorshift64star(wsp->duplicate_set[i]);
        	}
        }

        // Initialize the current unique seed.

        wsp->curr_unique_seed = wsp->init_unique_seed;
	}

//	uint64_t nsecs2 = get_nsecs();	// For performance tuning.

	// Find the LUN in the hash_to_LUN map.

	auto LUN_itr = wsp->hash_to_LUN.find(lun_hash);
	if (LUN_itr == wsp->hash_to_LUN.end()) {
		// It's possible that this LUN has been deleted already.
		mut.unlock();
		return 0;	// Generate a zero-filled block.
	}

	uint64_t LUN = LUN_itr->second;
	if (! wsp->LUN_assigned[LUN]) {
		mut.unlock();
		return 0;
	}

//	uint64_t nsecs3 = get_nsecs();	// For performance tuning.

	// Compute the seed. Ignore the first iosize block on each LUN. Calculate which group
	// this block is in, and the index within the group. (A group is one set of uniques
	// plus duplicates.)

	int index = (LBA - (wsp->iosize / wsp->dedupe_unit_bytes)) * wsp->LUN_count + wsp->LUN_to_index[LUN];
	int group = index / (wsp->uniques + wsp->duplicates);
	int block = index % (wsp->uniques + wsp->duplicates);

	// This (seed) is what we will return.

	uint64_t seed;

	// The LUN is divided into groups. The first portion of a group comprises unique blocks;
	// the second portion comprises duplicate blocks.

	if (block < wsp->uniques) {

		// This is a unique block, so simply generate another unique seed.
		// Note: This part of the method does *not* generate blocks that are static!

		xorshift64star(wsp->curr_unique_seed);
        seed = wsp->curr_unique_seed;
	} else {

		// This is a duplicate block.

		if (wsp->duplicate_set == nullptr) {

			// If there is no duplicate set, e.g., because allocation failed, just re-use
			// the most recent unique seed as a duplicate.  Note: This part of the method
			// does *not* generate blocks that are static!

	        seed = wsp->curr_unique_seed;
		} else {

			// There is a duplicate set. Get the appropriate duplicate set entry.
			// Note: This part of the method *does* generate blocks that are static!

			uint64_t dupi = (group * wsp->duplicates + block - wsp->uniques) % wsp->duplicate_set_size;
			seed = wsp->duplicate_set[dupi];
		}
	}

//	uint64_t nsecs4 = get_nsecs(); 	// For performance tuning.

//	if (logging)
//	{
//		ostringstream oss;
//		oss << __func__ << "(" << __LINE__ << "): "
//			<< "work_part = \"" << work_part << "\""
//			<< ", lun_part = \"" << lun_part << "\""
//			<< ", LUN_count = " << wsp->LUN_count
//			<< ", LUN_max_plus_one = " << wsp->LUN_max_plus_one
//			<< ", LUN = " << LUN
//			<< ", LUNi = " << wsp->LUN_to_LUN_index[LUN]
//			<< ", LBA = " << LBA
//			<< ", iosize = " << wsp->iosize
//			<< ", dss = " << wsp->duplicate_set_size
//			<< ", dub = " << wsp->dedupe_unit_bytes
//		    << ", ddr = " << wsp->dedupe_ratio
//		    << ", crf = " << wsp->correction_factor
//			<< ", uniques = " << wsp->uniques
//			<< ", duplicates = " << wsp->duplicates
//			<< ", group = " << group
//			<< ", block = " << block
//		    << ", seed = " << hex << showbase << internal << setfill('0') << setw(18) << seed << setfill(' ') << dec;
//		if (block >= wsp->uniques) {
//			oss << " (duplicate)";
//		}
//		oss << ", work lookup ns = " << nsecs1
//			<< ", dupset init ns = " << nsecs2
//			<< ", lun lookup ns = " << nsecs3
//			<< ", seed comp ns = " << nsecs4;
//		oss << "." << endl;
//		logger logfile {"/tmp/spm.txt"};
//		log(logfile, oss.str());
//	}

	mut.unlock();

	return seed;
}
