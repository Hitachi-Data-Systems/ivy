//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <linux/aio_abi.h>      /* for AIO types and constants */

typedef struct iogenerator_stuff {

	long long int LUN_size_bytes;

	int sector_size=512;

	int fd {-1};

	aio_context_t act{0};

} iogenerator_stuff;

