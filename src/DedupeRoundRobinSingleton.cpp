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

#include "ivytypes.h"
#include "ivydefines.h"
#include "ivyhelpers.h"
#include "DedupeRoundRobinSingleton.h"

void DedupeRoundRobinSingleton::init(void)
{
    initialized = false;

    LUN_count = 0;
    iosize = 0;
    dedupe_blocks = 0;
    dedupe_unit_bytes = 0;
    duplicate_set_size = 0;
    duplicates = 0;
    uniques = 0;
    init_duplic_seed = 0x0123456789abcdefULL;
    init_unique_seed = 0xfedcba9876543210ULL;
    curr_unique_seed = 0;
    if (duplicate_set != nullptr) {
    	free(duplicate_set);
        duplicate_set = nullptr;
    }
    dedupe_ratio = 0.0F;
    dedupe_correction_factor = 0.0F;
    LUN_to_LDEV.clear();
    LUN_to_LUN_index.clear();
}

void DedupeRoundRobinSingleton::compute_factors(void)
{
	assert(initialized);

	// Correct for impact of duplicate set on deduplication ratio.

    dedupe_correction_factor = ((ivy_float) dedupe_blocks / dedupe_ratio + (ivy_float) duplicate_set_size) /
    							((ivy_float) dedupe_blocks / dedupe_ratio);

    // Extract those parts of the corrected deduplication ratio both before and after the decimal point.

    uint64_t  rational_fixed = (uint64_t) (dedupe_ratio * dedupe_correction_factor / 1);
    ivy_float rational_float = (dedupe_ratio * dedupe_correction_factor) - (ivy_float) (rational_fixed * 1.0);

    // Compute a rational approximation to the part of the corrected dedupe_ratio after the decimal point.
    // This call limits the error to 0.000001 and the number of duplicates to 1,000.

    std::pair<uint64_t, uint64_t> pp = farey(rational_float, 0.000001, 1000);

    // Determine the number of uniques and duplicates in each group.

    uniques    = pp.second;
    duplicates = pp.first + pp.second * (uint64_t) (dedupe_ratio * dedupe_correction_factor - rational_float - 1.0);

    // Generate the LUN_to_LUN_index map from the LUN_to_LDEV map. Works even if some or all LDEVs are zero (0).
    // Note: This is very inefficient in the number of LUNs, but it's not in the main loop, so its efficiency doesn't matter.
    // Of course, for a very large number of LUNs, this could take awhile...

    LUN_to_LUN_index.clear();
    for (uint64_t i = 0; i < LUN_count; i++) {
    	unsigned LDEV_min = 0xffffffff;
    	uint64_t index = 0; // Make compiler happy.
    	auto it = LUN_to_LDEV.begin();
    	while (it != LUN_to_LDEV.end()) {
    		if (it->second < LDEV_min) {
    			auto it2 = LUN_to_LUN_index.begin();
    			while (it2 != LUN_to_LUN_index.end()) {
    				if (it2->first == it->first) {
    					goto next;
    				}
    				it2++;
    			}
    			LDEV_min = it->second;
    			index = it->first;
    		}
    	next:
    		it++;
    	}
    	LUN_to_LUN_index[index] = i;
    }
	if (false)
	{
		for (uint64_t i = 0; i < LUN_count; i++) {
			std::ostringstream oss;
			oss << __func__ << "(" << __LINE__ << "): "
				<< "LUN[" << i << "] = " << LUN_to_LUN_index[i]
				<< " (0x" << std::hex << std::setw(8) << std::setfill('0') << LUN_to_LDEV[i] << std::dec << std::setfill(' ') << ")"
				<< "." << std::endl;
			logger logfile {"/tmp/spm.txt"};
			log(logfile, oss.str());
		}
	}
}

uint64_t DedupeRoundRobinSingleton::add_LUN(uint64_t my_blocks, uint64_t my_blocksize,
										    ivy_float my_dedupe_ratio, uint64_t my_dedupe_unit_bytes,
											LUN *pLUN)
{
	mut.lock();

	assert(my_blocks > 0);
	assert(my_blocksize > 0);
	assert(my_dedupe_ratio >= 1.0);
	assert((my_dedupe_unit_bytes > 0) && (my_dedupe_unit_bytes % dedupe_unit_bytes_default == 0));
	assert (pLUN != nullptr);

	// Assume that the key parameters for this workload are the same for all LUNs.
	// If they're different, we'll get odd behavior.

	if (! initialized) {
		init();

		dedupe_ratio = my_dedupe_ratio;
		dedupe_unit_bytes = my_dedupe_unit_bytes;
		iosize = my_blocksize;

		initialized = true;
	}

	// Don't know what to do if the number of blocks in this coverage differ from other coverage(s) in this workload.

	dedupe_blocks += my_blocks * my_blocksize / my_dedupe_unit_bytes;

	// Keep updating the duplicate set size until the duplicate set has been allocated and initialized.

	if (duplicate_set == nullptr) {
		duplicate_set_size = (uint64_t) (dedupe_blocks * duplicate_set_size_ratio);
	}

	// Remember the number of LUNs in this workload as well as this LUN's index.

	uint64_t LUN = LUN_count++;

	// Find the LDEV (if any) associated with this LUN. Note: There will only be an LDEV if there is a command device connector.

	std::string ldev = pLUN->attribute_value(std::string("ldev"));
	trim(ldev);
	toUpper(ldev);

	unsigned LDEV = get_LDEV(ldev); // Will return (unsigned) 0 if ldev is empty or malformed string.
	LUN_to_LDEV[LUN] = LDEV;

	// Don't know whether this is the last LUN for this workload. Assume not and compute
	// factors necessary to generate seeds. Will update later if additional LUN(s) added.

	compute_factors();

	if (false)
	{
		std::ostringstream oss;
		oss << __func__ << "(" << __LINE__ << "): "
			<< "LUN = " << LUN;
		if 	(ldev.size() > 0) {
			oss << ", ldev = \"" << ldev << "\""
				<< ", LDEV = " << hex << showbase << internal << setfill('0') << setw(10) << LDEV << setfill(' ') << dec;
		}
		oss << ", blocks = " << my_blocks
			<< ", blocksize = " << my_blocksize
			<< ", dedupe_ratio = " << my_dedupe_ratio
			<< ", dedupe_unit = " << my_dedupe_unit_bytes
			<< ", dedupe_blocks = " << dedupe_blocks
			<< ", dup_set_size = " << duplicate_set_size
			<< ", dup_set_ptr = " << duplicate_set
			<< ", dedupe_correction_factor = " << dedupe_correction_factor
			<< ", uniques = " << uniques
			<< ", duplicates = " << duplicates
			<< ", act dedupe ratio = " << ((ivy_float) (uniques + duplicates) / uniques)
			<< "." << std::endl;
		logger logfile {"/tmp/spm.txt"};
		log(logfile, oss.str());
	}

	mut.unlock();

	return LUN;
}

void DedupeRoundRobinSingleton::del_LUN(uint64_t LUN)
{
	mut.lock();

	assert(initialized);
	assert(LUN_count > 0);

	// Discard a LUN. If final LUN, re-initialize the internal data fields. (This acts like a destructor.)

	if (--LUN_count == 0) {
		init();
	}

	if (false)
	{
		std::ostringstream oss;
		oss << __func__ << "(" << __LINE__ << "): "
			<< "LUN = " << LUN
			<< ", LUN_count = " << LUN_count
			<< ", initialized = " << initialized
			<< "." << std::endl;
		logger logfile {"/tmp/spm.txt"};
		log(logfile, oss.str());
	}

	mut.unlock();
}

uint64_t DedupeRoundRobinSingleton::get_dedupe_blocks(void)
{
	mut.lock();

	assert(initialized);

	// Return the number of dedupe blocks in this workload.

	uint64_t temp_blocks = dedupe_blocks;

	mut.unlock();

	return temp_blocks;
}

uint64_t DedupeRoundRobinSingleton::get_seed(uint64_t LUN, uint64_t LBA)
{
	mut.lock();

	assert(initialized);
	assert(LUN < LUN_count);

	// If the duplicate set is not yet instantiated, instantiate and initialize it.
	// This also initializes a bunch of other parameters, etc.

	if (duplicate_set == nullptr) {

		// If appropriate, allocate storage for the duplicate set.

		if (duplicate_set_size > 0) {
			duplicate_set = new (std::nothrow) uint64_t[duplicate_set_size];
		}

		// duplicate_set could be equal to nullptr if duplicate_set_size is zero (0) or new failed.

        if (duplicate_set != nullptr) {

        	// Initialize the duplicate set from the initial duplicate seed.

        	duplicate_set[0] = init_duplic_seed;
        	xorshift64star(duplicate_set[0]);

        	for (unsigned int i = 1; i < duplicate_set_size; i++)
        	{
        		duplicate_set[i] = duplicate_set[i-1];
        		xorshift64star(duplicate_set[i]);
        	}
        }

        // Initialize the current unique seed.

        curr_unique_seed = init_unique_seed;
	}

	// Ignore the first iosize block on each LUN. Calculate which group this block is in,
	// and the index within the group. (A group is one set of uniques plus duplicates.)

	uint64_t index = ((LBA - (iosize / dedupe_unit_bytes)) * LUN_count + LUN_to_LUN_index[LUN]);

	uint64_t group = index / (uniques + duplicates);
	uint64_t block = index % (uniques + duplicates);

	// This is what we will return.

	uint64_t seed;

	// The first portion of each group comprises unique blocks; the second portion comprises duplicates.

	if (block < uniques) {

		// This is a unique block, so simply generate another unique seed.
		// Note: This part of the method is *not* static!

		xorshift64star(curr_unique_seed);
        seed = curr_unique_seed;
	} else {

		// This is a duplicate block.

		if (duplicate_set == nullptr) {

			// If there is no duplicate set, just re-use the most recent unique seed as a duplicate.
			// Note: This part of the method is *not* static!

	        seed = curr_unique_seed;
		} else {

			// There is a duplicate set. Get the appropriate duplicate set entry.
			// Note: This part of the method *is* static!

			uint64_t dupi = (group * duplicates + block - uniques) % duplicate_set_size;
			seed = duplicate_set[dupi];
		}
	}

	if (false)
	{
		std::ostringstream oss;
		oss << __func__ << "(" << __LINE__ << "): "
			<< "LUN = " << LUN
			<< ", LUNi = " << LUN_to_LUN_index[LUN]
			<< ", LBA = " << LBA
			<< ", group = " << group
			<< ", block = " << block
		    << ", seed = " << hex << showbase << internal << setfill('0') << setw(18) << seed << setfill(' ') << dec;
		if (block >= uniques) {
			oss << " (duplicate)";
		}
		oss << "." << std::endl;
		logger logfile {"/tmp/spm.txt"};
		log(logfile, oss.str());
	}

	mut.unlock();

	return seed;
}
