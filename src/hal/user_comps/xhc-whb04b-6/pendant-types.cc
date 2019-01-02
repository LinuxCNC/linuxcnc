/*
   Copyright (C) 2018 Raoul Rubien (github.com/rubienr)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.
 */

#include "pendant-types.h"

// system includes
#include <assert.h>
#include <iostream>
#include <iomanip>

// 3rd party includes

// local library includes

// forward declarations

namespace XhcWhb04b6 {

// ----------------------------------------------------------------------

void HandWheelCounters::count(int8_t delta)
{
    auto idx = mActiveAxisCounter;
    if (mIsLeadCounterActive)
    {
        idx = CounterNameToIndex::LEAD;
    }

    if (CounterNameToIndex::UNDEFINED == idx)
    {
        return;
    }

    int32_t& counter = mCounters[static_cast<typename std::underlying_type<CounterNameToIndex>::type>(idx)];
    int32_t tmp = counter + delta;

    if (mIsLeadCounterActive)
    {
        tmp = (tmp < mLeadMinValue) ? mLeadMinValue : tmp;
        tmp = (tmp > mLeadMaxValue) ? mLeadMaxValue : tmp;
    }

    counter = tmp;
}

// ----------------------------------------------------------------------

HandWheelCounters::HandWheelCounters()
{
    mCounters[static_cast<typename std::underlying_type<CounterNameToIndex>::type>(CounterNameToIndex::LEAD)] = 100;
}

// ----------------------------------------------------------------------

int32_t HandWheelCounters::counts() const
{
    if (mIsLeadCounterActive)
    {
        auto idx = static_cast<typename std::underlying_type<CounterNameToIndex>::type>(CounterNameToIndex::LEAD);
        return mCounters[idx];
    }
    else
    {
        assert(mActiveAxisCounter != CounterNameToIndex::UNDEFINED);
        auto idx = static_cast<typename std::underlying_type<CounterNameToIndex>::type>(mActiveAxisCounter);
        return mCounters[idx];
    }
}

// ----------------------------------------------------------------------

int32_t HandWheelCounters::counts(CounterNameToIndex counterName) const
{
    auto idx = static_cast<typename std::underlying_type<CounterNameToIndex>::type>(counterName);
    return mCounters[idx];
}

// ----------------------------------------------------------------------

void HandWheelCounters::setActiveCounter(CounterNameToIndex activeMode)
{
    mActiveAxisCounter = activeMode;
}

// ----------------------------------------------------------------------

HandWheelCounters::CounterNameToIndex HandWheelCounters::activeCounter() const
{
    return mActiveAxisCounter;
}

// ----------------------------------------------------------------------

bool HandWheelCounters::isLeadCounterActive() const
{
    return mIsLeadCounterActive;
}

// ----------------------------------------------------------------------

void HandWheelCounters::enableLeadCounter(bool isEnabled)
{
    mIsLeadCounterActive = isEnabled;
}

// ----------------------------------------------------------------------

void HandWheelCounters::setLeadValueLimit(int32_t min, int32_t max)
{
    mLeadMinValue = min;
    mLeadMaxValue = max;
}

// ----------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const HandWheelCounters& data)
{
    std::ios init(NULL);
    init.copyfmt(os);
    if (data.isLeadCounterActive())
    {
        os << "{counts=" << data.counts()
           << " activeCounter=LEAD"
           << " isLeadActive=" << data.isLeadCounterActive() << "}";
    }
    else if (HandWheelCounters::CounterNameToIndex::UNDEFINED == data.activeCounter())
    {
        os << "{counts=NA"
           << " activeCounter=UNDEFINED"
           << " isLeadActive=" << data.isLeadCounterActive() << "}";
    }
    else
    {
        os << "{counts=" << data.counts()
           << " activeCounter="
           << static_cast<int16_t>(data.activeCounter())
           << " isLeadActive=" << data.isLeadCounterActive() << "}";
    }

    os.copyfmt(init);
    return os;
}
}
