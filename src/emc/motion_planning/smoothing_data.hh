/********************************************************************
* Description: smoothing_data.hh
*   Helper types for lookahead buffer and velocity smoothing
*
* Author: Tormach (original), Port by LinuxCNC community
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2022-2026 All rights reserved.
*
********************************************************************/
#ifndef SMOOTHING_DATA_HH
#define SMOOTHING_DATA_HH

#include <vector>

// Maximum lookahead depth (safety limit for pre-allocation)
#define MAX_LOOKAHEAD_DEPTH 200

// Type alias for consistency with Tormach implementation
using SmoothingVector = std::vector<double>;

// SmoothingData is defined in motion_planning_9d.hh
// This header just provides the type alias for use in implementation files

#endif // SMOOTHING_DATA_HH
