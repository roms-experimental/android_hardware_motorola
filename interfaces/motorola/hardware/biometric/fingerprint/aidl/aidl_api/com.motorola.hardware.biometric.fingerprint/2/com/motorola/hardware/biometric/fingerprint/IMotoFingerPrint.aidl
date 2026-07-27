/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.motorola.hardware.biometric.fingerprint;

import com.motorola.hardware.biometric.fingerprint.FingerHardWareInfo;
import com.motorola.hardware.biometric.fingerprint.IMotoEventResult;
import com.motorola.hardware.biometric.fingerprint.IMotoFingerprintCallback;
import com.motorola.hardware.biometric.fingerprint.IMotoFodEventType;

@VintfStability
interface IMotoFingerPrint {
    int cancel();
    String getCheckinVersion();
    FingerHardWareInfo getHardwareInfo();
    String[] getUnlockPerformanceData();
    IMotoEventResult sendCommand(in int commandId, in byte[] param);
    IMotoEventResult sendFodEvent(in IMotoFodEventType eventType, in @nullable byte[] eventData);
    void setNotify(in IMotoFingerprintCallback cb);
}
