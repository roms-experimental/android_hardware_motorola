/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.motorola.hardware.biometric.fingerprint;

import com.motorola.hardware.biometric.fingerprint.IMotoCaptiveSensorTestCallback;

@VintfStability
interface IMotoCaptiveSensorTest {
    int beginSensorTest(in IMotoCaptiveSensorTestCallback callback);
    void checkerboardTest();
    void finishSensorTest(in int testId);
    void imagequalityTest();
    void otpvalidationTest();
    void selfTest();
}
