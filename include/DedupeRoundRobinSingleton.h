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
#pragma once

#include <assert.h>
#include <time.h>

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "ivydriver.h"
#include "LUN.h"
#include "Workload.h"

class DedupeRoundRobinSingleton
{
    private:

		static const bool debugging {true};
		static const bool logging {false};

		// Note that these are *private*. This class is a *singleton*, so its objects
		// cannot be constructed or destructed!

        DedupeRoundRobinSingleton() {}
        ~DedupeRoundRobinSingleton() {}

        // Multiple threads will call this class's entry points simultaneously,
        // so we need to protect with a mutex.

        mutex mut;

        // This is a magic constant that was determined by experiment. Don't change it!
        
        static constexpr ivy_float duplicate_set_size_ratio {0.050};

        // There is one workload_state structure created for each workload.
        
        struct workload_state {
            bool initialized;				// The initialized field indicates whether the other fields have valid values.
            int	 LUN_count;					// How many LUNs participate in this workload. Can be sparse.
            int	 LUN_max_plus_one;			// One more than the maximum LUN number used in the workload.
            int  duplicate_set_size;    	// The size of the duplicate set. Typically 0.005 * dedupe_blocks, but can be smaller.
            int  uniques;        			// The computed number of uniques in a "group."
            int  duplicates;        		// The computed number of duplicates in a "group."
            uint64_t iosize;        		// The size, in bytes, of an I/O (a write) for this workload.
            uint64_t dedupe_blocks;     	// The number of deduplication-size blocks for all LUNs in this workload.
            uint64_t dedupe_unit_bytes;     // The size, in bytes, of a deduplication unit. Typically 8,192.
            uint64_t init_duplic_seed;      // The initial seed for duplicates (not modified).
            uint64_t init_unique_seed;      // The initial seed for uniques (modified).
            uint64_t curr_unique_seed;		// The current seed for unique blocks.
            uint64_t *duplicate_set;        // A pointer to the duplicate set, which comprises a set of 64-bit seeds used to generate duplicate blocks.
            ivy_float dedupe_ratio;        	// The deduplication ratio used for this workload.
            ivy_float correction_factor;	// A correction factor applied to dedupe_ratio, based on the duplicate set size and the number of covered blocks.
            string work_part;				// The workload identifier part of the workloadID string.
            map<int, bool> LUN_assigned;	// Map from assigned LUN number to whether LUN is assigned.
            map<int, unsigned> LUN_to_LDEV;	// Map from assigned LUN number to its LDEV number.
            map<int, int> LUN_to_index;		// Map from assigned LUN number to its index number.
            map<uint64_t, int> hash_to_LUN;	// Map from LUN hash to assigned LUN number.
        };

        using MyWorkloadMap = map<uint64_t, struct workload_state *>;

        // workload_map is the map of workloads, indexed by a hash of the workload string.

        MyWorkloadMap workload_map;

		//
		// Farey algorithm for computing a good rational approximation.
		//
		// @param val A value between 0.0 and 1.0 to be approximated by two integers (a/b).
		// @param err The required precision such that abs(val-a/b) < err. (err > 0.0).
		// @param num The maximum size of the numerator allowed. (num > 0)
		// 
		static pair<uint64_t, uint64_t> farey(ivy_float val, ivy_float err, uint64_t num)
		{
			if (debugging) {
				assert((val >= 0.0) && (val <= 1.0));
				assert(err > 0.0);
				assert(num > 0);
			}

		    uint64_t a(0), b(1), c(1), d(1);
		    ivy_float mediant(0.0F);

		    while ((b <= num) && (d <= num)) {
		        mediant = static_cast<ivy_float> (a + c) / static_cast<ivy_float> (b + d);

		        if (abs(val - mediant) < err) {
		            if (b + d <= num) {
		                return pair<uint64_t, uint64_t> (a + c, b + d);
		            } else if (d > b) {
		                return pair<uint64_t, uint64_t> (c, d);
		            } else {
		                return pair<uint64_t, uint64_t> (a, b);
		            }
		        } else if (val > mediant) {
		            a = a + c;
		            b = b + d;
		        } else {
		            c = a + c;
		            d = b + d;
		        }
		    }

		    if (b > num) {
		        return pair<uint64_t, uint64_t> (c, d);
		    } else {
		        return pair<uint64_t, uint64_t> (a, b);
		    }
		}

		// Convert a string LDEV to an LDEV value, e.g., "01:0A" to 0x0001000A.

		static unsigned get_ldev(string ldev)
		{
			// Format of ldev string should be "[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]", e.g., "01:2B".

			const int max_ldev_length = 5;
			unsigned ldev_value = 0;

			if (ldev.length() == max_ldev_length) {
				for (int i = 0; i < max_ldev_length; i++) {
					switch (i) {
					case 2:
						// Don't know what to do if LDEV contains invalid character--so skip this character.
						if (debugging) {
							assert(ldev.at(2) == ':'); // For debugging, only!
						}
						break;
					default:
						// Get character, convert to value, and add result to LDEV_value.
						char ldev_char = ldev.at(i);
						ldev_value <<= 8;
						if ((ldev_char >= '0') && (ldev_char <= '9')) {
							ldev_value += (ldev_char - '0');
						} else if ((ldev_char >= 'A') && (ldev_char <= 'F')) {
							ldev_value += (ldev_char - 'A' + 10);
						} else {
							// Don't know what to do if LDEV contains invalid character--so skip this character.
							if (debugging) {
								assert(false); // For debugging, only!
							}
						}
						break;
					}
				}
			}

			return ldev_value;
		}

		// Compute the deduplication factors from the dedupe_ratio, etc.

		void compute_factors(MyWorkloadMap::iterator itr);

		// Initialize the fields of the workload structure.

		void init(MyWorkloadMap::iterator itr);

		// Return the number of nanoseconds elapsed since the previous call to this method.

//		static inline uint64_t get_nsecs(void) {
//			static uint64_t nsecs = 0;
//			uint64_t tmp, diff;
//			struct timespec ts;
//
//			if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
//				return 0;
//			}
//
//			tmp = (ts.tv_sec * 1000000000ULL + ts.tv_nsec);
//			diff = tmp - nsecs;
//			nsecs = tmp;
//
//			return diff;
//		}

    public:

		// Return a reference to the DedupeRoundRobinSingleton.

		static DedupeRoundRobinSingleton& get_instance(void)
		{
			static DedupeRoundRobinSingleton instance;
		    return instance;
		}

		// Add a LUN to this workload.

		void add_lun(string &host_part, string &lun_part, string &work_part,
					 uint64_t host_hash, uint64_t lun_hash, uint64_t work_hash,
					 uint64_t my_blocks, uint64_t my_blocksize, ivy_float my_dedupe_ratio,
					 uint64_t my_dedupe_unit_bytes, LUN *pLUN);

		// Delete a LUN from this workload.

		void del_lun(string &host_part, string &lun_part, string &work_part,
					 uint64_t host_hash, uint64_t lun_hash, uint64_t work_hash);

		// Given a LUN and an LBA within the LUN, generate a seed. Takes into account the zero ratio,
		// the dedupe ratio, etc. Note: If zero is returned, a zero-filled block should be generated.

		uint64_t get_seed(string &host_part, string &lun_part, string &work_part,
						  uint64_t host_hash, uint64_t lun_hash, uint64_t work_hash,
						  uint64_t LBA);
		
		// Don't instantiate these!

        DedupeRoundRobinSingleton(DedupeRoundRobinSingleton const&) = delete;

        void operator=(DedupeRoundRobinSingleton const&) = delete;

    protected:

};
