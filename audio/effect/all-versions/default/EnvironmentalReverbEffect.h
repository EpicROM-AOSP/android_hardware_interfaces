/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ANDROID_HARDWARE_AUDIO_EFFECT_ENVIRONMENTALREVERBEFFECT_H
#define ANDROID_HARDWARE_AUDIO_EFFECT_ENVIRONMENTALREVERBEFFECT_H

#include <system/audio_effects/effect_environmentalreverb.h>

#include PATH(android/hardware/audio/effect/FILE_VERSION/IEnvironmentalReverbEffect.h)

#include "Effect.h"

#include <system/audio_effects/effect_environmentalreverb.h>

#include <hidl/Status.h>

#include <hidl/MQDescriptor.h>

#include "VersionUtils.h"

namespace android {
namespace hardware {
namespace audio {
namespace effect {
namespace CPP_VERSION {
namespace implementation {

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using namespace ::android::hardware::audio::common::COMMON_TYPES_CPP_VERSION;
using namespace ::android::hardware::audio::effect::CPP_VERSION;

struct EnvironmentalReverbEffect : public IEnvironmentalReverbEffect {
    explicit EnvironmentalReverbEffect(effect_handle_t handle);

    // Methods from ::android::hardware::audio::effect::CPP_VERSION::IEffect follow.
    Return<Result> init() override;
    Return<Result> setConfig(
        const EffectConfig& config, const sp<IEffectBufferProviderCallback>& inputBufferProvider,
        const sp<IEffectBufferProviderCallback>& outputBufferProvider) override;
    Return<Result> reset() override;
    Return<Result> enable() override;
    Return<Result> disable() override;
#if MAJOR_VERSION <= 6
    Return<Result> setAudioSource(AudioSource source) override;
    Return<Result> setDevice(AudioDeviceBitfield device) override;
    Return<Result> setInputDevice(AudioDeviceBitfield device) override;
#else
    Return<Result> setAudioSource(const AudioSource& source) override;
    Return<Result> setDevice(const DeviceAddress& device) override;
    Return<Result> setInputDevice(const DeviceAddress& device) override;
#endif
    Return<void> setAndGetVolume(const hidl_vec<uint32_t>& volumes,
                                 setAndGetVolume_cb _hidl_cb) override;
    Return<Result> volumeChangeNotification(const hidl_vec<uint32_t>& volumes) override;
    Return<Result> setAudioMode(AudioMode mode) override;
    Return<Result> setConfigReverse(
        const EffectConfig& config, const sp<IEffectBufferProviderCallback>& inputBufferProvider,
        const sp<IEffectBufferProviderCallback>& outputBufferProvider) override;
    Return<void> getConfig(getConfig_cb _hidl_cb) override;
    Return<void> getConfigReverse(getConfigReverse_cb _hidl_cb) override;
    Return<void> getSupportedAuxChannelsConfigs(
        uint32_t maxConfigs, getSupportedAuxChannelsConfigs_cb _hidl_cb) override;
    Return<void> getAuxChannelsConfig(getAuxChannelsConfig_cb _hidl_cb) override;
    Return<Result> setAuxChannelsConfig(const EffectAuxChannelsConfig& config) override;
    Return<Result> offload(const EffectOffloadParameter& param) override;
    Return<void> getDescriptor(getDescriptor_cb _hidl_cb) override;
    Return<void> prepareForProcessing(prepareForProcessing_cb _hidl_cb) override;
    Return<Result> setProcessBuffers(const AudioBuffer& inBuffer,
                                     const AudioBuffer& outBuffer) override;
    Return<void> command(uint32_t commandId, const hidl_vec<uint8_t>& data, uint32_t resultMaxSize,
                         command_cb _hidl_cb) override;
    Return<Result> setParameter(const hidl_vec<uint8_t>& parameter,
                                const hidl_vec<uint8_t>& value) override;
    Return<void> getParameter(const hidl_vec<uint8_t>& parameter, uint32_t valueMaxSize,
                              getParameter_cb _hidl_cb) override;
    Return<void> getSupportedConfigsForFeature(uint32_t featureId, uint32_t maxConfigs,
                                               uint32_t configSize,
                                               getSupportedConfigsForFeature_cb _hidl_cb) override;
    Return<void> getCurrentConfigForFeature(uint32_t featureId, uint32_t configSize,
                                            getCurrentConfigForFeature_cb _hidl_cb) override;
    Return<Result> setCurrentConfigForFeature(uint32_t featureId,
                                              const hidl_vec<uint8_t>& configData) override;
    Return<Result> close() override;
    Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) override;

    // Methods from
    // ::android::hardware::audio::effect::CPP_VERSION::IEnvironmentalReverbEffect follow.
    Return<Result> setBypass(bool bypass) override;
    Return<void> getBypass(getBypass_cb _hidl_cb) override;
    Return<Result> setRoomLevel(int16_t roomLevel) override;
    Return<void> getRoomLevel(getRoomLevel_cb _hidl_cb) override;
    Return<Result> setRoomHfLevel(int16_t roomHfLevel) override;
    Return<void> getRoomHfLevel(getRoomHfLevel_cb _hidl_cb) override;
    Return<Result> setDecayTime(uint32_t decayTime) override;
    Return<void> getDecayTime(getDecayTime_cb _hidl_cb) override;
    Return<Result> setDecayHfRatio(int16_t decayHfRatio) override;
    Return<void> getDecayHfRatio(getDecayHfRatio_cb _hidl_cb) override;
    Return<Result> setReflectionsLevel(int16_t reflectionsLevel) override;
    Return<void> getReflectionsLevel(getReflectionsLevel_cb _hidl_cb) override;
    Return<Result> setReflectionsDelay(uint32_t reflectionsDelay) override;
    Return<void> getReflectionsDelay(getReflectionsDelay_cb _hidl_cb) override;
    Return<Result> setReverbLevel(int16_t reverbLevel) override;
    Return<void> getReverbLevel(getReverbLevel_cb _hidl_cb) override;
    Return<Result> setReverbDelay(uint32_t reverbDelay) override;
    Return<void> getReverbDelay(getReverbDelay_cb _hidl_cb) override;
    Return<Result> setDiffusion(int16_t diffusion) override;
    Return<void> getDiffusion(getDiffusion_cb _hidl_cb) override;
    Return<Result> setDensity(int16_t density) override;
    Return<void> getDensity(getDensity_cb _hidl_cb) override;
    Return<Result> setAllProperties(
        const IEnvironmentalReverbEffect::AllProperties& properties) override;
    Return<void> getAllProperties(getAllProperties_cb _hidl_cb) override;

   private:
    sp<Effect> mEffect;

    virtual ~EnvironmentalReverbEffect() = default;

    void propertiesFromHal(const t_reverb_settings& halProperties,
                           IEnvironmentalReverbEffect::AllProperties* properties);
    void propertiesToHal(const IEnvironmentalReverbEffect::AllProperties& properties,
                         t_reverb_settings* halProperties);
};

}  // namespace implementation
}  // namespace CPP_VERSION
}  // namespace effect
}  // namespace audio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_AUDIO_EFFECT_ENVIRONMENTALREVERBEFFECT_H
