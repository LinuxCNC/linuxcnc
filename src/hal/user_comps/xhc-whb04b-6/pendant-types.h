/*
   Copyright (C) 2017 Raoul Rubien (github.com/rubienr)

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

#pragma once

// system includes
#include <stdint.h>
#include <type_traits>
#include <iosfwd>

// 3rd party includes

// local library includes

// forward declarations


namespace XhcWhb04b6 {

// ----------------------------------------------------------------------

class HandwheelStepmodes
{
public:
    enum class Mode : uint8_t
    {
        CONTINUOUS  = 0,
        STEP        = 1,
        //LEAD        = 2, // remove, unused
        MODES_COUNT = 2
    };
};

// ----------------------------------------------------------------------

class HandWheelCounters
{
public:

    enum class CounterNameToIndex : uint8_t
    {
        AXIS_X,
        AXIS_Y,
        AXIS_Z,
        AXIS_A,
        AXIS_B,
        AXIS_C,
        LEAD,
        COUNTERS_COUNT,
        UNDEFINED
    };

    HandWheelCounters();

    void count(int8_t delta);
    //! Return the currently active counter which is in-/decreased by \xrefitem count(uint8_t).
    //! The current counter mode is set via \xrefitem setModeActive(CounterNameToIndex).
    //! \return the accumulated counter
    int32_t counts() const;
    //! Returns the counter which is in-/decreased by \xrefitem count(uint8_t).
    //! \param counterName the counter value to return
    //! \return the accumulated counter
    int32_t counts(CounterNameToIndex counterName) const;
    void setActiveCounter(CounterNameToIndex activeMode);
    CounterNameToIndex activeCounter() const;
    bool isLeadCounterActive() const;
    void enableLeadCounter(bool isEnabled);
    void setLeadValueLimit(int32_t min, int32_t max);

protected:
    bool               mIsLeadCounterActive{false};
    CounterNameToIndex mActiveAxisCounter{HandWheelCounters::CounterNameToIndex::UNDEFINED};
    int32_t            mCounters[static_cast<typename std::underlying_type<CounterNameToIndex>::type>(CounterNameToIndex::COUNTERS_COUNT)]{
        0
    };
    int32_t            mLeadMinValue{0};
    int32_t            mLeadMaxValue{150};

private:
};

std::ostream& operator<<(std::ostream& os, const HandWheelCounters& data);
}
