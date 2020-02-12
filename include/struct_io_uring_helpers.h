#pragma once

#include <cstdint>

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
