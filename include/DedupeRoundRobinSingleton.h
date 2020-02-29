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

		static const bool debugging {false};
		static const bool logging {false};

		// Note that these are *private*. This class a *singleton*, so its objects cannot be constructed or destructed!

        DedupeRoundRobinSingleton() {}
        ~DedupeRoundRobinSingleton() {}

        // Multiple threads will call this class's entry points simultaneously, so we need to protect with a mutex.

        mutex mut;

        // This is a magic constant that was determined by experiment. Don't change it!
        
        static constexpr ivy_float duplicate_set_size_ratio {0.050};
        
        struct workload_state {
            bool initialized;				// The initialized field indicates whether the other fields have valid values.
            uint64_t LUN_count;				// How many LUNs participate in this workload.
            uint64_t iosize;        		// The size, in bytes, of an I/O (a write) for this workload.
            uint64_t dedupe_blocks;     	// The number of deduplication-size blocks for all LUNs in this workload.
            uint64_t dedupe_unit_bytes;     // The size, in bytes, of a deduplication unit. Typically 8,192.
            uint64_t duplicate_set_size;    // The size of the duplicate set. Typically 0.005 * dedupe_blocks, but can be smaller.
            uint64_t duplicates;        	// The computed number of duplicates in a "group."
            uint64_t uniques;        		// The computed number of uniques in a "group."
            uint64_t init_duplic_seed;      // The initial seed for duplicates (not modified).
            uint64_t init_unique_seed;      // The initial seed for uniques (modified).
            uint64_t curr_unique_seed;		// The current seed.
            uint64_t *duplicate_set;        // A pointer to the duplicate set, which comprises a set of 64-bit seeds used to generate duplicate blocks.
            ivy_float dedupe_ratio;        	// The deduplication ratio used for this workload.
            ivy_float correction_factor;	// A correction factor applied to dedupe_ratio, based on the duplicate set size and the number of covered blocks.
            string ident_part;				// The identifier part of the workload.
            map<uint64_t, bool> LUN_assigned;			// Map from assigned LUN number to whether active.
            map<uint64_t, unsigned> LUN_to_LDEV;		// Map from assigned LUN number to LDEV number.
            map<uint64_t, uint64_t> LUN_to_LUN_index;	// Map from assigned LUN number to LUN index number.
            map<uint64_t, uint64_t> hash_to_LUN;			// Map from LUN hash to assigned LUN number.
        };

        using MyWorkloadMap = map<uint64_t, struct workload_state *>;
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

		static unsigned get_LDEV(string LDEV)
		{
			// Format of LDEV string should be "[0-9A-F][0-9A-F]:[0-9A-F][0-9A-F]", e.g., "01:2B".

			const int MAX_LDEV_LENGTH = 5;

			// If bad length, pretend LDEV is "00:00".

			if (LDEV.length() != MAX_LDEV_LENGTH) {
				return 0;
			}

			unsigned LDEV_value = 0;

			for (int i = 0; i < MAX_LDEV_LENGTH; i++) {

				switch (i) {

				case 2:

					// Don't know what to do if LDEV contains invalid character--so skip this character.
					if (debugging) {
						assert(LDEV.at(2) == ':'); // For debugging, only!
					}
					break;

				default:

					// Get character, convert to value, and add result to LDEV_value.

					char LDEV_char = LDEV.at(i);

					if ((LDEV_char >= '0') && (LDEV_char <= '9')) {
						LDEV_value += (LDEV_char - '0') << 24;
					} else if ((LDEV_char >= 'A') && (LDEV_char <= 'F')) {
						LDEV_value += (LDEV_char - 'A' + 10) << 24;
					} else {
						// Don't know what to do if LDEV contains invalid character--so skip this character.
						if (debugging) {
							assert(false); // For debugging, only!
						}
					}
					break;
				}
			}

			return LDEV_value;
		}

		// Called to compute the deduplication factors from the dedupe_ratio, etc.

		void compute_factors(MyWorkloadMap::iterator itr);

		// Initializes the fields of this singleton.

		void init(MyWorkloadMap::iterator itr);

		static inline uint64_t get_nsecs(void) {
			static uint64_t nsecs = 0;
			uint64_t tmp, diff;
			struct timespec ts;

			if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
				return 0;
			}

			tmp = (ts.tv_sec * 1000000000ULL + ts.tv_nsec);
			diff = tmp - nsecs;
			nsecs = tmp;

			return diff;
		}

    public:

		// Returns a reference to this singleton.

		static DedupeRoundRobinSingleton& get_instance(void)
		{
			static DedupeRoundRobinSingleton instance;
			
		    return instance;
		}

		// Add a LUN to this workload.

		void add_LUN(string &host_part, string &lun_part, string &work_part,
					 uint64_t host_hash, uint64_t lun_hash, uint64_t work_hash,
					 uint64_t my_blocks, uint64_t my_blocksize, ivy_float my_dedupe_ratio,
					 uint64_t my_dedupe_unit_bytes, LUN *pLUN);

		// Delete a LUN from this workload.

		void del_LUN(string &host_part, string &lun_part, string &work_part,
					 uint64_t host_hash, uint64_t lun_hash, uint64_t work_hash);

		// Given a LUN and an LBA within the LUN, generate a seed. Takes into account the zero ratio, the dedupe ratio, etc.

		uint64_t get_seed(string &host_part, string &lun_part, string &work_part,
						  uint64_t host_hash, uint64_t lun_hash, uint64_t work_hash,
						  uint64_t LBA);
		
		// Don't instantiate these!

        DedupeRoundRobinSingleton(DedupeRoundRobinSingleton const&) = delete;

        void operator=(DedupeRoundRobinSingleton const&) = delete;

    protected:

};
