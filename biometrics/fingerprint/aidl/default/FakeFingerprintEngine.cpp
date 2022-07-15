/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FakeFingerprintEngine.h"

#include <android-base/logging.h>

#include <fingerprint.sysprop.h>

#include "util/CancellationSignal.h"
#include "util/Util.h"

using namespace ::android::fingerprint::virt;

namespace aidl::android::hardware::biometrics::fingerprint {

void FakeFingerprintEngine::generateChallengeImpl(ISessionCallback* cb) {
    BEGIN_OP(0);
    std::uniform_int_distribution<int64_t> dist;
    auto challenge = dist(mRandom);
    FingerprintHalProperties::challenge(challenge);
    cb->onChallengeGenerated(challenge);
}

void FakeFingerprintEngine::revokeChallengeImpl(ISessionCallback* cb, int64_t challenge) {
    BEGIN_OP(0);
    FingerprintHalProperties::challenge({});
    cb->onChallengeRevoked(challenge);
}

void FakeFingerprintEngine::enrollImpl(ISessionCallback* cb,
                                       const keymaster::HardwareAuthToken& hat,
                                       const std::future<void>& cancel) {
    BEGIN_OP(FingerprintHalProperties::operation_enroll_latency().value_or(DEFAULT_LATENCY));

    // Do proper HAT verification in the real implementation.
    if (hat.mac.empty()) {
        LOG(ERROR) << "Fail: hat";
        cb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorError */);
        return;
    }

    if (FingerprintHalProperties::operation_enroll_fails().value_or(false)) {
        LOG(ERROR) << "Fail: operation_enroll_fails";
        cb->onError(Error::VENDOR, 0 /* vendorError */);
        return;
    }

    // format is "<id>:<progress_ms>,<progress_ms>,...:<result>
    auto nextEnroll = FingerprintHalProperties::next_enrollment().value_or("");
    auto parts = Util::split(nextEnroll, ":");
    if (parts.size() != 3) {
        LOG(ERROR) << "Fail: invalid next_enrollment";
        cb->onError(Error::VENDOR, 0 /* vendorError */);
        return;
    }
    auto enrollmentId = std::stoi(parts[0]);
    auto progress = Util::split(parts[1], ",");
    for (size_t i = 0; i < progress.size(); i++) {
        auto left = progress.size() - i - 1;
        SLEEP_MS(std::stoi(progress[i]));

        if (shouldCancel(cancel)) {
            LOG(ERROR) << "Fail: cancel";
            cb->onError(Error::CANCELED, 0 /* vendorCode */);
            return;
        }

        cb->onAcquired(AcquiredInfo::GOOD, 0 /* vendorCode */);
        if (left == 0 && !IS_TRUE(parts[2])) {  // end and failed
            LOG(ERROR) << "Fail: requested by caller: " << nextEnroll;
            FingerprintHalProperties::next_enrollment({});
            cb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorCode */);
        } else {  // progress and update props if last time
            if (left == 0) {
                auto enrollments = FingerprintHalProperties::enrollments();
                enrollments.emplace_back(enrollmentId);
                FingerprintHalProperties::enrollments(enrollments);
                FingerprintHalProperties::next_enrollment({});
                LOG(INFO) << "Enrolled: " << enrollmentId;
            }
            cb->onEnrollmentProgress(enrollmentId, left);
        }
    }
}

void FakeFingerprintEngine::authenticateImpl(ISessionCallback* cb, int64_t /* operationId */,
                                             const std::future<void>& cancel) {
    BEGIN_OP(FingerprintHalProperties::operation_authenticate_latency().value_or(DEFAULT_LATENCY));

    auto now = Util::getSystemNanoTime();
    int64_t duration = FingerprintHalProperties::operation_authenticate_duration().value_or(0);
    do {
        if (FingerprintHalProperties::operation_authenticate_fails().value_or(false)) {
            LOG(ERROR) << "Fail: operation_authenticate_fails";
            cb->onError(Error::VENDOR, 0 /* vendorError */);
            return;
        }

        if (FingerprintHalProperties::lockout().value_or(false)) {
            LOG(ERROR) << "Fail: lockout";
            cb->onLockoutPermanent();
            cb->onError(Error::HW_UNAVAILABLE, 0 /* vendorError */);
            return;
        }

        if (shouldCancel(cancel)) {
            LOG(ERROR) << "Fail: cancel";
            cb->onError(Error::CANCELED, 0 /* vendorCode */);
            return;
        }

        auto id = FingerprintHalProperties::enrollment_hit().value_or(0);
        auto enrolls = FingerprintHalProperties::enrollments();
        auto isEnrolled = std::find(enrolls.begin(), enrolls.end(), id) != enrolls.end();
        if (id > 0 && isEnrolled) {
            cb->onAuthenticationSucceeded(id, {} /* hat */);
            return;
        }

        SLEEP_MS(100);
    } while (!Util::hasElapsed(now, duration));

    LOG(ERROR) << "Fail: not enrolled";
    cb->onAuthenticationFailed();
    cb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorError */);
}

void FakeFingerprintEngine::detectInteractionImpl(ISessionCallback* cb,
                                                  const std::future<void>& cancel) {
    BEGIN_OP(FingerprintHalProperties::operation_detect_interaction_latency().value_or(
            DEFAULT_LATENCY));

    if (FingerprintHalProperties::operation_detect_interaction_fails().value_or(false)) {
        LOG(ERROR) << "Fail: operation_detect_interaction_fails";
        cb->onError(Error::VENDOR, 0 /* vendorError */);
        return;
    }

    if (shouldCancel(cancel)) {
        LOG(ERROR) << "Fail: cancel";
        cb->onError(Error::CANCELED, 0 /* vendorCode */);
        return;
    }

    auto id = FingerprintHalProperties::enrollment_hit().value_or(0);
    auto enrolls = FingerprintHalProperties::enrollments();
    auto isEnrolled = std::find(enrolls.begin(), enrolls.end(), id) != enrolls.end();
    if (id <= 0 || !isEnrolled) {
        LOG(ERROR) << "Fail: not enrolled";
        cb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorError */);
        return;
    }

    cb->onInteractionDetected();
}

void FakeFingerprintEngine::enumerateEnrollmentsImpl(ISessionCallback* cb) {
    BEGIN_OP(0);

    std::vector<int32_t> ids;
    for (auto& enrollment : FingerprintHalProperties::enrollments()) {
        auto id = enrollment.value_or(0);
        if (id > 0) {
            ids.push_back(id);
        }
    }

    cb->onEnrollmentsEnumerated(ids);
}

void FakeFingerprintEngine::removeEnrollmentsImpl(ISessionCallback* cb,
                                                  const std::vector<int32_t>& enrollmentIds) {
    BEGIN_OP(0);

    std::vector<std::optional<int32_t>> newEnrollments;
    std::vector<int32_t> removed;
    for (auto& enrollment : FingerprintHalProperties::enrollments()) {
        auto id = enrollment.value_or(0);
        if (std::find(enrollmentIds.begin(), enrollmentIds.end(), id) != enrollmentIds.end()) {
            removed.push_back(id);
        } else if (id > 0) {
            newEnrollments.emplace_back(id);
        }
    }
    FingerprintHalProperties::enrollments(newEnrollments);

    cb->onEnrollmentsRemoved(enrollmentIds);
}

void FakeFingerprintEngine::getAuthenticatorIdImpl(ISessionCallback* cb) {
    BEGIN_OP(0);
    cb->onAuthenticatorIdRetrieved(FingerprintHalProperties::authenticator_id().value_or(0));
}

void FakeFingerprintEngine::invalidateAuthenticatorIdImpl(ISessionCallback* cb) {
    BEGIN_OP(0);
    auto id = FingerprintHalProperties::authenticator_id().value_or(0);
    auto newId = id + 1;
    FingerprintHalProperties::authenticator_id(newId);
    cb->onAuthenticatorIdInvalidated(newId);
}

void FakeFingerprintEngine::resetLockoutImpl(ISessionCallback* cb,
                                             const keymaster::HardwareAuthToken& /*hat*/) {
    BEGIN_OP(0);
    FingerprintHalProperties::lockout(false);
    cb->onLockoutCleared();
}

}  // namespace aidl::android::hardware::biometrics::fingerprint