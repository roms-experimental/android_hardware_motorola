/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.motorola.hardware.biometric.fingerprint;

import com.motorola.hardware.biometric.fingerprint.IMotoEventResult;
import com.motorola.hardware.biometric.fingerprint.TestResult;

@VintfStability
interface IMotoFodSensorTestCallback {
    void onCalibrationStepTestResult(in IMotoEventResult result);
    void onCheckCalibrationStatusResult(in IMotoEventResult result);
    void onImageQualityTestResult(in IMotoEventResult result, in int quality);
    void onSelfTestResult(in IMotoEventResult result, in TestResult testResult);
}
