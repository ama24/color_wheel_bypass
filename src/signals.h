#pragma once

// ===========================================================================
//  signals.h — SENS and LDUP hardware timer outputs
// ===========================================================================

// SENS — colour-wheel index pulse (~120 Hz, 91.2% duty)
// Call sens_start() on LDPCN rising edge.
void sens_start();
void sens_stop();

// LDUP — multi-phase light-output PWM
// Active only during lamp-on (same timing as LLITZ).
void ldup_start();
void ldup_stop();
