/*
 * Copyright (C) 2024-2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <thread>

#include "Legacy2Aidl.h"
#include "Session.h"

#include "CancellationSignal.h"

namespace aidl::android::hardware::biometrics::fingerprint {

void onClientDeath(void* cookie) {
    ALOGI("FingerprintService has died");
    Session* session = static_cast<Session*>(cookie);
    if (session && !session->isClosed()) {
        session->close();
    }
}

Session::Session(fingerprint_device_t* device, UdfpsHandler* udfpsHandler, int userId,
                 std::shared_ptr<ISessionCallback> cb, LockoutTracker lockoutTracker,
                 std::vector<SensorLocation> sensorLocations)
    : mDevice(device),
      mLockoutTracker(lockoutTracker),
      mUserId(userId),
      mCb(cb),
      mUdfpsHandler(udfpsHandler),
      mSensorLocations(std::move(sensorLocations)) {
    mDeathRecipient = AIBinder_DeathRecipient_new(onClientDeath);

    auto path = std::format("/data/vendor_de/{}/fpdata/", userId);
    mDevice->set_active_group(mDevice, mUserId, path.c_str());
}

ndk::ScopedAStatus Session::generateChallenge() {
    uint64_t challenge = mDevice->pre_enroll(mDevice);
    ALOGI("generateChallenge: %ld", challenge);
    mCb->onChallengeGenerated(challenge);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::revokeChallenge(int64_t challenge) {
    ALOGI("revokeChallenge: %ld", challenge);
    mDevice->post_enroll(mDevice);
    mCb->onChallengeRevoked(challenge);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::enroll(const HardwareAuthToken& hat,
                                   std::shared_ptr<ICancellationSignal>* out) {
    hw_auth_token_t authToken;
    translate(hat, authToken);
    int error = mDevice->enroll(mDevice, &authToken, mUserId, 60);
    if (error) {
        ALOGE("enroll failed: %d", error);
        mCb->onError(Error::UNABLE_TO_PROCESS, error);
    }

    *out = SharedRefBase::make<CancellationSignal>(this);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::authenticate(int64_t operationId,
                                         std::shared_ptr<ICancellationSignal>* out) {
    checkSensorLockout();
    int error = mDevice->authenticate(mDevice, operationId, mUserId);
    if (error) {
        ALOGE("authenticate failed: %d", error);
        mCb->onError(Error::UNABLE_TO_PROCESS, error);
    }

    *out = SharedRefBase::make<CancellationSignal>(this);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::detectInteraction(std::shared_ptr<ICancellationSignal>* out) {
    ALOGD("Detect interaction is not supported");
    mCb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorCode */);

    *out = SharedRefBase::make<CancellationSignal>(this);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::enumerateEnrollments() {
    int error = mDevice->enumerate(mDevice);
    if (error) {
        ALOGE("enumerate failed: %d", error);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::removeEnrollments(const std::vector<int32_t>& enrollmentIds) {
    ALOGI("removeEnrollments, size: %zu", enrollmentIds.size());

    if (enrollmentIds.empty()) {
        int error = mDevice->remove(mDevice, mUserId, 0);
        if (error) {
            ALOGE("remove failed: %d", error);
        }
    } else {
        for (int32_t fid : enrollmentIds) {
            int error = mDevice->remove(mDevice, mUserId, fid);
            if (error) {
                ALOGE("remove failed: %d", error);
            }
        }
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::getAuthenticatorId() {
    uint64_t auth_id = mDevice->get_authenticator_id(mDevice);
    ALOGI("getAuthenticatorId: %ld", auth_id);
    mCb->onAuthenticatorIdRetrieved(auth_id);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::invalidateAuthenticatorId() {
    uint64_t auth_id = mDevice->get_authenticator_id(mDevice);
    ALOGI("invalidateAuthenticatorId: %ld", auth_id);
    mCb->onAuthenticatorIdInvalidated(auth_id);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::resetLockout(const HardwareAuthToken& /*hat*/) {
    clearLockout(true);
    if (mIsLockoutTimerStarted) mIsLockoutTimerAborted = true;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerDown(int32_t /*pointerId*/, int32_t x, int32_t y, float minor,
                                          float major) {
    if (mUdfpsHandler) {
        mUdfpsHandler->onFingerDown(x, y, minor, major);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerUp(int32_t /*pointerId*/) {
    if (mUdfpsHandler) {
        mUdfpsHandler->onFingerUp();
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onUiReady() {
    if (mUdfpsHandler) {
        if (!mSensorLocations.empty()) {
            mUdfpsHandler->onFingerDown(mSensorLocations[0].sensorLocationX,
                                        mSensorLocations[0].sensorLocationY,
                                        mSensorLocations[0].sensorRadius,
                                        mSensorLocations[0].sensorRadius);
        } else {
            mUdfpsHandler->onFingerDown(0, 0, 0, 0);
        }
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::authenticateWithContext(
        int64_t operationId, const common::OperationContext& /*context*/,
        std::shared_ptr<common::ICancellationSignal>* out) {
    return authenticate(operationId, out);
}

ndk::ScopedAStatus Session::enrollWithContext(const keymaster::HardwareAuthToken& hat,
                                              const common::OperationContext& /*context*/,
                                              std::shared_ptr<common::ICancellationSignal>* out) {
    return enroll(hat, out);
}

ndk::ScopedAStatus Session::detectInteractionWithContext(
        const common::OperationContext& /*context*/,
        std::shared_ptr<common::ICancellationSignal>* out) {
    return detectInteraction(out);
}

ndk::ScopedAStatus Session::onPointerDownWithContext(const PointerContext& context) {
    return onPointerDown(context.pointerId, context.x, context.y, context.minor, context.major);
}

ndk::ScopedAStatus Session::onPointerUpWithContext(const PointerContext& context) {
    return onPointerUp(context.pointerId);
}

ndk::ScopedAStatus Session::onContextChanged(const common::OperationContext& /*context*/) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerCancelWithContext(const PointerContext& /*context*/) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::setIgnoreDisplayTouches(bool /*shouldIgnore*/) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::cancel() {
    if (mUdfpsHandler) {
        mUdfpsHandler->cancel();
    }

    int ret = mDevice->cancel(mDevice);

    if (ret == 0) {
        mCb->onError(Error::CANCELED, 0 /* vendorCode */);
        return ndk::ScopedAStatus::ok();
    }

    return ndk::ScopedAStatus::fromServiceSpecificError(ret);
}

ndk::ScopedAStatus Session::close() {
    mClosed = true;
    mCb->onSessionClosed();
    AIBinder_DeathRecipient_delete(mDeathRecipient);
    return ndk::ScopedAStatus::ok();
}

binder_status_t Session::linkToDeath(AIBinder* binder) {
    return AIBinder_linkToDeath(binder, mDeathRecipient, this);
}

bool Session::isClosed() {
    return mClosed;
}

// Translate from errors returned by traditional HAL (see fingerprint.h) to
// AIDL-compliant Error
Error Session::VendorErrorFilter(int32_t error, int32_t* vendorCode) {
    *vendorCode = 0;

    switch (error) {
        case FINGERPRINT_ERROR_HW_UNAVAILABLE:
        case FINGERPRINT_ERROR_VENDOR_BASE + 1:
        case FINGERPRINT_ERROR_VENDOR_BASE + 2:
        case FINGERPRINT_ERROR_VENDOR_BASE + 3:
            return Error::HW_UNAVAILABLE;
        case FINGERPRINT_ERROR_UNABLE_TO_PROCESS:
        case FINGERPRINT_ERROR_VENDOR_BASE + 4:
        case FINGERPRINT_ERROR_VENDOR_BASE + 5:
            return Error::UNABLE_TO_PROCESS;
        case FINGERPRINT_ERROR_TIMEOUT:
            return Error::TIMEOUT;
        case FINGERPRINT_ERROR_NO_SPACE:
            return Error::NO_SPACE;
        case FINGERPRINT_ERROR_CANCELED:
            return Error::CANCELED;
        case FINGERPRINT_ERROR_UNABLE_TO_REMOVE:
            return Error::UNABLE_TO_REMOVE;
        case FINGERPRINT_ERROR_LOCKOUT: {
            *vendorCode = FINGERPRINT_ERROR_LOCKOUT;
            return Error::VENDOR;
        }
        default:
            if (error >= FINGERPRINT_ERROR_VENDOR_BASE) {
                // vendor specific code.
                *vendorCode = error - FINGERPRINT_ERROR_VENDOR_BASE;
                return Error::VENDOR;
            }
    }
    ALOGE("Unknown error from fingerprint vendor library: %d", error);
    return Error::UNABLE_TO_PROCESS;
}

// Translate acquired messages returned by traditional HAL (see fingerprint.h)
// to AIDL-compliant AcquiredInfo
AcquiredInfo Session::VendorAcquiredFilter(int32_t info, int32_t* vendorCode) {
    *vendorCode = 0;

    switch (info) {
        case FINGERPRINT_ACQUIRED_GOOD:
            return AcquiredInfo::GOOD;
        case FINGERPRINT_ACQUIRED_PARTIAL:
            return AcquiredInfo::PARTIAL;
        case FINGERPRINT_ACQUIRED_INSUFFICIENT:
            return AcquiredInfo::INSUFFICIENT;
        case FINGERPRINT_ACQUIRED_IMAGER_DIRTY:
            return AcquiredInfo::SENSOR_DIRTY;
        case FINGERPRINT_ACQUIRED_TOO_SLOW:
            return AcquiredInfo::TOO_SLOW;
        case FINGERPRINT_ACQUIRED_TOO_FAST:
        case FINGERPRINT_ACQUIRED_VENDOR_BASE + 8:
            return AcquiredInfo::TOO_FAST;
        case FINGERPRINT_ACQUIRED_VENDOR_BASE + 5:
        case FINGERPRINT_ACQUIRED_VENDOR_BASE + 6:
        case FINGERPRINT_ACQUIRED_VENDOR_BASE + 7:
            *vendorCode = info - FINGERPRINT_ACQUIRED_VENDOR_BASE;
            return AcquiredInfo::VENDOR;
        default:
            if (info >= FINGERPRINT_ACQUIRED_VENDOR_BASE) {
                // vendor specific code.
                *vendorCode = info - FINGERPRINT_ACQUIRED_VENDOR_BASE;
                return AcquiredInfo::VENDOR;
            }
    }
    ALOGE("Unknown acquired message from fingerprint vendor library: %d", info);
    return AcquiredInfo::UNKNOWN;
}

bool Session::checkSensorLockout() {
    LockoutTracker::LockoutMode lockoutMode = mLockoutTracker.getMode();
    if (lockoutMode == LockoutTracker::LockoutMode::kPermanent) {
        ALOGE("Fail: lockout permanent");
        mCb->onLockoutPermanent();
        mIsLockoutTimerAborted = true;
        return true;
    }
    if (lockoutMode == LockoutTracker::LockoutMode::kTimed) {
        int64_t timeLeft = mLockoutTracker.getLockoutTimeLeft();
        ALOGE("Fail: lockout timed: %ld", timeLeft);
        mCb->onLockoutTimed(timeLeft);
        if (!mIsLockoutTimerStarted) startLockoutTimer(timeLeft);
        return true;
    }
    return false;
}

void Session::clearLockout(bool clearAttemptCounter) {
    mLockoutTracker.reset(clearAttemptCounter);
    mCb->onLockoutCleared();
}

void Session::startLockoutTimer(int64_t timeout) {
    std::function<void()> action = std::bind(&Session::lockoutTimerExpired, this);
    std::thread([timeout, action]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        action();
    }).detach();

    mIsLockoutTimerStarted = true;
}

void Session::lockoutTimerExpired() {
    if (!mIsLockoutTimerAborted) clearLockout(false);

    mIsLockoutTimerStarted = false;
    mIsLockoutTimerAborted = false;
}

void Session::notify(const fingerprint_msg_t* msg) {
    // const uint64_t devId = reinterpret_cast<uint64_t>(mDevice);
    switch (msg->type) {
        case FINGERPRINT_ERROR: {
            int32_t vendorCode = 0;
            Error result = VendorErrorFilter(msg->data.error, &vendorCode);
            ALOGD("onError(%hhd, %d)", result, vendorCode);
            mCb->onError(result, vendorCode);
        } break;
        case FINGERPRINT_ACQUIRED: {
            int32_t vendorCode = 0;
            AcquiredInfo result =
                    VendorAcquiredFilter(msg->data.acquired.acquired_info, &vendorCode);
            ALOGD("onAcquired(%hhd, %d)", result, vendorCode);
            if (mUdfpsHandler) {
                mUdfpsHandler->onAcquired(static_cast<int32_t>(result), vendorCode);
            }
            // Filter specific vendor codes that cause framework to incorrectly disable
            // UDFPS display mode during processing. Pass through other vendor messages
            // that may contain useful user feedback.
            if (result == AcquiredInfo::VENDOR && (vendorCode >= 5 && vendorCode <= 7)) {
                ALOGD("Filtering vendor acquired code %d", vendorCode);
            } else {
                mCb->onAcquired(result, vendorCode);
            }
        } break;
        case FINGERPRINT_TEMPLATE_ENROLLING: {
            ALOGD("onEnrollResult(fid=%d, gid=%d, rem=%d)", msg->data.enroll.finger.fid,
                  msg->data.enroll.finger.gid, msg->data.enroll.samples_remaining);
            mCb->onEnrollmentProgress(msg->data.enroll.finger.fid,
                                      msg->data.enroll.samples_remaining);
        } break;
        case FINGERPRINT_TEMPLATE_REMOVED: {
            std::vector<int32_t> enrollments;
            enrollments.reserve(NUM_FINGERS);
            for (unsigned int i = 0; i < NUM_FINGERS; i++) {
                int32_t fid = msg->data.removed.fingers[i].fid;
                if (!fid) break;
                ALOGD("onRemove(fid=%d)", fid);
                enrollments.push_back(fid);
            }
            mCb->onEnrollmentsRemoved(enrollments);
        } break;
        case FINGERPRINT_AUTHENTICATED: {
            ALOGD("onAuthenticated(fid=%d, gid=%d)", msg->data.authenticated.finger.fid,
                  msg->data.authenticated.finger.gid);
            if (msg->data.authenticated.finger.fid != 0) {
                const hw_auth_token_t hat = msg->data.authenticated.hat;
                HardwareAuthToken authToken;
                translate(hat, authToken);

                if (mUdfpsHandler) {
                    mUdfpsHandler->onAuthenticationSucceeded();
                }
                mCb->onAuthenticationSucceeded(msg->data.authenticated.finger.fid, authToken);
                mLockoutTracker.reset(true);
            } else {
                if (mUdfpsHandler) {
                    mUdfpsHandler->onAuthenticationFailed();
                }
                mCb->onAuthenticationFailed();
                mLockoutTracker.addFailedAttempt();
                checkSensorLockout();
            }
        } break;
        case FINGERPRINT_TEMPLATE_ENUMERATING: {
            std::vector<int32_t> enrollments;
            enrollments.reserve(NUM_FINGERS);
            for (unsigned int i = 0; i < NUM_FINGERS; i++) {
                int32_t fid = msg->data.enumerated.fingers[i].fid;
                if (!fid) break;
                ALOGD("onEnumerate(fid=%d)", fid);
                enrollments.push_back(fid);
            }
            mCb->onEnrollmentsEnumerated(enrollments);
        } break;
        case FINGERPRINT_GENERATE_CHALLENGE: {
            int64_t challenge = msg->data.data;
            ALOGI("onChallengeGenerated: %ld", challenge);
            mCb->onChallengeGenerated(challenge);
        } break;
        case FINGERPRINT_REVOKE_CHALLENGE: {
            int64_t challenge = msg->data.data;
            ALOGI("onChallengeRevoked: %ld", challenge);
            mCb->onChallengeRevoked(challenge);
        } break;
        case FINGERPRINT_GET_AUTHENTICATOR_ID: {
            int auth_id = msg->data.data;
            ALOGI("onAuthenticatorIDRetrieved: %d", auth_id);
            if (mUdfpsHandler) {
                mUdfpsHandler->onFingerUp();
            }
            mCb->onAuthenticatorIdRetrieved(auth_id);
        } break;
        case FINGERPRINT_INVALIDATE_AUTHENTICATOR_ID: {
            int64_t new_auth_id = msg->data.data;
            ALOGI("onAuthenticatorIDInvalidated, new auth id: %ld", new_auth_id);
            mCb->onAuthenticatorIdInvalidated(new_auth_id);
        } break;
    }
}

}  // namespace aidl::android::hardware::biometrics::fingerprint
