#ifndef __HAL_HALQREC_HH
#define __HAL_HALQREC_HH
//
// HAL Python query API
//
// Copyright (c) 2026  B.Stultiens
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#include <stddef.h>
#include <stdlib.h>
#include <stdexcept>
#include <fmt/format.h>
#include <hal.h>

//
// Result recording class
// To collect copies of hal_query_t structures in a HAL query callback.
//
// Uses malloc and friends to avoid throwing exceptions in the collection part.
// However, it does throw exceptions on initial construction and if you try to
// address an invalid recorded record. In both cases it throws a
// std::runtime_error with an appropriate message.
//
// The allocated array is cleaned up as soon as the instance is destructed.
//
class HalQRec
{
public:
    HalQRec(size_t nmin = 64)
        : n(0), na(nmin), qr(nullptr)
    {
        if(na < 1)
            na = 1;
        qr = static_cast<hal_query_t *>(calloc(na, sizeof(*qr)));
        if(!qr)
            throw std::runtime_error(fmt::format("HalQRec: failed to calloc {} hal_query_t elements", na));
    }

    ~HalQRec()
    {
        if(qr)
            free(qr);
    }

    // It is important that the append() method does not throw exceptions. It
    // is called with the HAL mutex locked and we cannot exit the callback
    // uncontrolled. Otherwise, we'd have HAL permanently locked.
    // The append() method returns zero (0) on success or -ENOMEM when out of
    // memory.
    int append(const hal_query_t *q) noexcept {
        if(n >= na){
            hal_query_t *qrn = static_cast<hal_query_t *>(reallocarray(qr, na * 2, sizeof(*qr)));
            if(!qrn)
                return -ENOMEM;
            qr = qrn;
            na *= 2;
            memset(&qr[n], 0, (na - n) * sizeof(*qr));
        }
        qr[n] = *q;
        n++;
        return 0;
    }

    //
    // Generic callback collecting all results copying them over in
    // a results array for later examination.
    //
    static int get_qrec_cb(hal_query_t *q, void *arg) noexcept {
        return reinterpret_cast<HalQRec *>(arg)->append(q);
    }

    size_t size() const { return n; }
    size_t maxsize() const { return na; }
    const hal_query_t *rec(size_t i) const {
        if(i < n)
            return &qr[i];
        if(n > 0)
            throw std::runtime_error(fmt::format("HalQRec: Index {} out of range [0,{}]", i, n-1));
        else
            throw std::runtime_error(fmt::format("HalQRec: Index {} out of range, no entries available", i));
    }

private:
    size_t n;
    size_t na;
    hal_query_t *qr;

    HalQRec(const HalQRec &) = delete;
    HalQRec &operator=(const HalQRec &) = delete;
};

#endif
