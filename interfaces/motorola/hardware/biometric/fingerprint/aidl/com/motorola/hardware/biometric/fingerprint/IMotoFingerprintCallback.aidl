/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.motorola.hardware.biometric.fingerprint;

@VintfStability
interface IMotoFingerprintCallback {
    void ipc_callback(in int cmdId, in int arg1, in int arg2, in byte[] data, in int result);
    void onDaemonMessage(in long devId, in int msgId, in int arg1, in byte[] data);
}
