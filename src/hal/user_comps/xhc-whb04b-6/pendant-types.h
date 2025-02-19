/*
   Copyright (C) 2018 Raoul Rubien (github.com/rubienr) Updated for Linuxcnc 2020 by alkabal_free.fr

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

#ifndef __XHC_WHB04B_6_PENDANT_TYPES_H
#define __XHC_WHB04B_6_PENDANT_TYPES_H

// system includes
#include <stdint.h>
#include <type_traits>
#include <iosfwd>


// forward declarations


namespace XhcWhb04b6 {
// ----------------------------------------------------------------------
class HandwheelStepmodes
{
public:
    enum class Mode : uint8_t
    {
        CON     = 0,
        STEP    = 1,
        MPG     = 2,
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

protected:
    bool               mIsLeadCounterActive{false};
    CounterNameToIndex mActiveAxisCounter{HandWheelCounters::CounterNameToIndex::UNDEFINED};
    int32_t            mCounters[static_cast<typename std::underlying_type<CounterNameToIndex>::type>(CounterNameToIndex::UNDEFINED)]{
        0
    };

private:
};

std::ostream& operator<<(std::ostream& os, const HandWheelCounters& data);
}
#endif
