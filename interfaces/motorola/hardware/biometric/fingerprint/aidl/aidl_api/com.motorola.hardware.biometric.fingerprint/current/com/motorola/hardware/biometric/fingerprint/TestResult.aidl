/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.motorola.hardware.biometric.fingerprint;

@VintfStability
parcelable TestResult {
    String[] testNames;
    int[] testResults;
    int status;
}
