//Copyright (c) 2016-2020 Hitachi Vantara Corporation
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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

#include "ivytypes.h"

#include <liburing.h>

inline uint32_t net_new_sqe_quantity(const struct io_uring* p_ur) // the3 ones that the kernel doesn't know abo9ut yet
{
    return p_ur->sq.sqe_tail >= *(p_ur->sq.ktail)
    ?      p_ur->sq.sqe_tail -  *(p_ur->sq.ktail)
    :      1 + (UINT32_MAX - (*(p_ur->sq.ktail) - p_ur->sq.sqe_tail)); // wraparound case
}

inline uint32_t my_total_sq_quantity(const struct io_uring* p_ur)
{
    return p_ur->sq.sqe_tail >= p_ur->sq.sqe_head
    ?      p_ur->sq.sqe_tail -  p_ur->sq.sqe_head
    :      1 + (UINT32_MAX - (p_ur->sq.sqe_head - p_ur->sq.sqe_tail)); // wraparound case
}

inline uint32_t k_sq_quantity(const struct io_uring* p_ur)
{
    return *(p_ur->sq.ktail) >= *(p_ur->sq.khead)
    ?      *(p_ur->sq.ktail) -  *(p_ur->sq.khead)
    : 1 + (UINT32_MAX - (*(p_ur->sq.khead) - *(p_ur->sq.ktail)));
}

inline uint32_t k_cq_quantity(const struct io_uring* p_ur)
{
    return *(p_ur->cq.ktail) >= *(p_ur->cq.khead)
    ?      *(p_ur->cq.ktail) -  *(p_ur->cq.khead)
    : 1 + (UINT32_MAX - (*(p_ur->cq.khead) - *(p_ur->cq.ktail)));
}
