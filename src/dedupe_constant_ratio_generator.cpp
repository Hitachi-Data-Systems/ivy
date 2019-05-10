//Copyright (c) 2019 Hitachi Vantara Corporation
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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>, Stephen Morgan <stephen.morgan@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

// This utility is used to populate the "dedupe_table" in DedupeConstantRatioRegulator.cpp.
// It generates an expected dedupe ratio for a given number of slots, assuming a 64-sided die.
// Unneeded entries (lines) may be pruned from the output, for compactness.  Note: Doing so won't
// appreciably speed up the code as binary search is used on the table.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int
cmpint(const void *p1, const void *p2)
{
	if (*(int *) p1 < *(int *) p2)
		return -1;
	else if (*(int *) p1 > *(int *) p2)
		return 1;
	else
		return 0;
}

float
dedupe(int *array, int size)
{
	int i, unique;

	assert(size > 0);
	assert(array);

	// First, sort the array.

	qsort(array, size, sizeof(int), cmpint);

	// Count the number of unique elements.

	unique = 1;
	for (i = 1; i < size; i++)
	{
		if (array[i] != array[i - 1])
			unique++;
	}

	// Compute the deduplication ratio.

	return (float) size / unique;
}

#define SIDES	(64)
#define SLOTS	(641)
#define TRIES	(10000)

int
main(void)
{
	float avg_dedupe;
	int array[SLOTS];
	int trial;
	int slots;

	srand48(time(NULL));

	for (slots = 1; slots < SLOTS; slots++)
	{
		avg_dedupe = 0.0;

		for (trial = 0; trial < TRIES; trial++)
		{
			int i;

			memset((void *) array, '\0', sizeof(array));

			for (i = 0; i < slots; i++)
			{
				array[i] = (int) (drand48() * SIDES);
			}

			avg_dedupe += dedupe(array, slots);
		}
		avg_dedupe = (float) (avg_dedupe / TRIES);

		fprintf(stdout, "\t{%d, %f},\n", slots, avg_dedupe);
	}

	return EXIT_SUCCESS;
}
