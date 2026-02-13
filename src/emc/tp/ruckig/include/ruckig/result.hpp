#pragma once


namespace ruckig {

//! Result type of Ruckig's update function
enum Result {
    Working = 0, ///< The trajectory is calculated normally
    Finished = 1, ///< The trajectory has reached its final position
    Error = -1, ///< Unclassified error
    ErrorInvalidInput = -100, ///< Error in the input parameter
    ErrorTrajectoryDuration = -101, ///< The trajectory duration exceeds its numerical limits
    ErrorPositionalLimits = -102, ///< The trajectory exceeds the given positional limits (only in Ruckig Pro)
    ErrorZeroLimits = -104, ///< The trajectory is not valid due to a conflict with zero limits
    ErrorExecutionTimeCalculation = -110, ///< Error during the extremel time calculation (Step 1)
    ErrorSynchronizationCalculation = -111, ///< Error during the synchronization calculation (Step 2)
};

}
