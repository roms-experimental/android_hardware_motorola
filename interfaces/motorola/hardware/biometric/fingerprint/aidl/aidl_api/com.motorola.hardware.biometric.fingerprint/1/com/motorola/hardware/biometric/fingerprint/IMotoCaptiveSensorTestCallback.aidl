/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.motorola.hardware.biometric.fingerprint;

import com.motorola.hardware.biometric.fingerprint.IMotoEventResult;

@VintfStability
interface IMotoCaptiveSensorTestCallback {
    void onCheckerboardTestResult(in IMotoEventResult result);
    void onGetSensorInfoResult(in IMotoEventResult result, in @nullable String info);
    void onImageQualityTestResult(in IMotoEventResult result);
    void onOtpvalidationTestResult(in IMotoEventResult result);
    void onSelfTestResult(in IMotoEventResult result);
}
