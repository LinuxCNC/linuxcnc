/*
 * cruckig - Pure C99 port of the Ruckig trajectory generation library
 * Copyright (c) 2025 Yang Yang <mika-net@outlook.com>
 * Copyright (c) 2021 Lars Berscheid (original C++ Ruckig)
 *
 * License: MIT, see the LICENSE file in this directory.
 */
#ifndef CRUCKIG_RESULT_H
#define CRUCKIG_RESULT_H

typedef enum {
    CRuckigWorking = 0,
    CRuckigFinished = 1,
    CRuckigError = -1,
    CRuckigErrorInvalidInput = -100,
    CRuckigErrorTrajectoryDuration = -101,
    CRuckigErrorPositionalLimits = -102,
    CRuckigErrorZeroLimits = -104,
    CRuckigErrorExecutionTimeCalculation = -110,
    CRuckigErrorSynchronizationCalculation = -111
} CRuckigResult;

typedef enum {
    CRuckigPosition = 0,
    CRuckigVelocity = 1
} CRuckigControlInterface;

typedef enum {
    CRuckigSyncTime = 0,
    CRuckigSyncTimeIfNecessary = 1,
    CRuckigSyncPhase = 2,
    CRuckigSyncNone = 3
} CRuckigSynchronization;

typedef enum {
    CRuckigContinuous = 0,
    CRuckigDiscrete = 1
} CRuckigDurationDiscretization;

#endif /* CRUCKIG_RESULT_H */
