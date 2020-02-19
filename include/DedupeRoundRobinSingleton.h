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

#include "LUN.h"

class DedupeRoundRobinSingleton
{
    private:

		// Note that these are *private*. This class a *singleton*, so its objects cannot be constructed or destructed!

        DedupeRoundRobinSingleton() {}
        ~DedupeRoundRobinSingleton() {}

        // Multiple threads will call this class's entry points simultaneously, so we need to protect with a mutex.

        std::mutex mut;

        // This is a magic constant that was determined by experiment. Don't change it!
        
        const ivy_float duplicate_set_size_ratio {0.050};
        
        // The initialized field indicates whether the other fields have valid values.

        bool initialized {false};

        // How many LUNs participate in this workload.

        uint64_t LUN_count;

        // The size, in bytes, of an I/O (a write) for this workload.

        uint64_t iosize;

        // The number of deduplication-size blocks for all LUNs in this workload.

        uint64_t dedupe_blocks;

        // The size, in bytes, of a deduplication unit. Typically 8,192.

        uint64_t dedupe_unit_bytes;

        // The size of the duplicate set. Typically 0.005 * dedupe_blocks, but can be smaller.

        uint64_t duplicate_set_size;

        // The computed number of duplicates in a "group."

        uint64_t duplicates;

        // The computed number of uniques in a "group."

        uint64_t uniques;

        // The initial seed for duplicates (not modified).

        uint64_t init_duplic_seed;

        // The initial seed for uniques (modified).

        uint64_t init_unique_seed;
        uint64_t curr_unique_seed;

        // A pointer to the duplicate set, which comprises a set of 64-bit seeds used to generate
        // duplicate blocks.

        uint64_t *duplicate_set {nullptr};

        // The deduplication ratio used for this workload.

        ivy_float dedupe_ratio;

        // A correction factor applied to dedupe_ratio, based on the duplicate set size and the
        // number of blocks to be covered by the deduplication code (dedupe_blocks).

        ivy_float dedupe_correction_factor;

        // The first map maps from assigned LUN number to LDEV number. The second map maps from assigned LUN number
        // to index number. The secnd map is built from the first.

        map<uint64_t, unsigned> LUN_to_LDEV;
        map<uint64_t, uint64_t> LUN_to_LUN_index;

		//
		// Farey algorithm for computing a good rational approximation.
		//
		// @param val A value between 0.0 and 1.0 to be approximated by two integers (a/b).
		// @param err The required precision such that abs(val-a/b) < err. (err > 0.0).
		// @param num The maximum size of the numerator allowed. (num > 0)
		// 
		static std::pair<uint64_t, uint64_t> farey(ivy_float val, ivy_float err, uint64_t num)
		{
		    assert((val >= 0.0) && (val <= 1.0));
		    assert(err > 0.0);
		    assert(num > 0);

		    uint64_t a(0), b(1), c(1), d(1);
		    ivy_float mediant(0.0F);

		    while ((b <= num) && (d <= num)) {
		        mediant = static_cast<ivy_float> (a + c) / static_cast<ivy_float> (b + d);

		        if (abs(val - mediant) < err) {
		            if (b + d <= num) {
		                return std::pair<uint64_t, uint64_t> (a + c, b + d);
		            } else if (d > b) {
		                return std::pair<uint64_t, uint64_t> (c, d);
		            } else {
		                return std::pair<uint64_t, uint64_t> (a, b);
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
		        return std::pair<uint64_t, uint64_t> (c, d);
		    } else {
		        return std::pair<uint64_t, uint64_t> (a, b);
		    }
		}

		// Convert a string LDEV to an LDEV value, e.g., "01:0A" to 0x0001000A.

		static unsigned get_LDEV(std::string LDEV)
		{
			if (LDEV.length() != 5) {
				return 0;
			}

			unsigned LDEV_value = 0;

			char LDEV_char = LDEV.at(0);

			if ((LDEV_char >= '0') && (LDEV_char <= '9')) {
				LDEV_value += (LDEV_char - '0') << 24;
			} else if ((LDEV_char >= 'A') && (LDEV_char <= 'F')) {
				LDEV_value += (LDEV_char - 'A' + 10) << 24;
			} else {
				assert(false);
			}

			LDEV_char = LDEV.at(1);

			if ((LDEV_char >= '0') && (LDEV_char <= '9')) {
				LDEV_value += (LDEV_char - '0') << 16;
			} else if ((LDEV_char >= 'A') && (LDEV_char <= 'F')) {
				LDEV_value += (LDEV_char - 'A' + 10) << 16;
			} else {
				assert(false);
			}

			LDEV_char = LDEV.at(3);

			if ((LDEV_char >= '0') && (LDEV_char <= '9')) {
				LDEV_value += (LDEV_char - '0') << 8;
			} else if ((LDEV_char >= 'A') && (LDEV_char <= 'F')) {
				LDEV_value += (LDEV_char - 'A' + 10) << 8;
			} else {
				assert(false);
			}

			LDEV_char = LDEV.at(4);

			if ((LDEV_char >= '0') && (LDEV_char <= '9')) {
				LDEV_value += (LDEV_char - '0') << 0;
			} else if ((LDEV_char >= 'A') && (LDEV_char <= 'F')) {
				LDEV_value += (LDEV_char - 'A' + 10) << 0;
			} else {
				assert(false);
			}

			return LDEV_value;
		}

		// Called to compute the deduplication factors from the dedupe_ratio, etc.

		void compute_factors(void);

    public:

		// Returns a reference to this singleton.

		static DedupeRoundRobinSingleton& get_instance(void)
		{
			static DedupeRoundRobinSingleton instance;
			
		    return instance;
		}

		// Initializes the fields of this singleton.

		void init(void);

		// Add a LUN to this workload.

		uint64_t add_LUN(uint64_t my_blocks, uint64_t my_blocksize, ivy_float my_dedupe_ratio, uint64_t my_dedupe_unit_bytes, LUN *pLUN);

		// Delete a LUN from this workload.

		void del_LUN(uint64_t LUN_number);

		// Retrieve the number of dedupe blocks covered by this workload.

		uint64_t get_dedupe_blocks(void);

		// Given a LUN and an LBA within the LUN, generate a seed. Takes into account the zero ratio, the dedupe ratio, etc.

		uint64_t get_seed(uint64_t LUN, uint64_t LBA);
		
		// Don't instantiate these!

        DedupeRoundRobinSingleton(DedupeRoundRobinSingleton const&) = delete;
        void operator=(DedupeRoundRobinSingleton const&) = delete;
};
