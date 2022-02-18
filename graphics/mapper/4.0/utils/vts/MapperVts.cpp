/*
 * Copyright 2019 The Android Open Source Project
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

#include <gralloctypes/Gralloc4.h>
#include <mapper-vts/4.0/MapperVts.h>

namespace android {
namespace hardware {
namespace graphics {
namespace mapper {
namespace V4_0 {
namespace vts {

Gralloc::Gralloc(const std::string& allocatorServiceName, const std::string& mapperServiceName,
                 bool errOnFailure) {
    if (errOnFailure) {
        init(allocatorServiceName, mapperServiceName);
    } else {
        initNoErr(allocatorServiceName, mapperServiceName);
    }
}

void Gralloc::init(const std::string& allocatorServiceName, const std::string& mapperServiceName) {
    mAllocator = IAllocator::getService(allocatorServiceName);
    ASSERT_NE(nullptr, mAllocator.get()) << "failed to get allocator service";

    mMapper = IMapper::getService(mapperServiceName);
    ASSERT_NE(nullptr, mMapper.get()) << "failed to get mapper service";
    ASSERT_FALSE(mMapper->isRemote()) << "mapper is not in passthrough mode";
}

void Gralloc::initNoErr(const std::string& allocatorServiceName,
                        const std::string& mapperServiceName) {
    mAllocator = IAllocator::getService(allocatorServiceName);

    mMapper = IMapper::getService(mapperServiceName);
    if (mMapper.get()) {
        ASSERT_FALSE(mMapper->isRemote()) << "mapper is not in passthrough mode";
    }
}

Gralloc::~Gralloc() {
    for (auto bufferHandle : mClonedBuffers) {
        auto buffer = const_cast<native_handle_t*>(bufferHandle);
        native_handle_close(buffer);
        native_handle_delete(buffer);
    }
    mClonedBuffers.clear();

    for (auto bufferHandle : mImportedBuffers) {
        auto buffer = const_cast<native_handle_t*>(bufferHandle);
        EXPECT_EQ(Error::NONE, mMapper->freeBuffer(buffer)) << "failed to free buffer " << buffer;
    }
    mImportedBuffers.clear();
}

sp<IAllocator> Gralloc::getAllocator() const {
    return mAllocator;
}

const native_handle_t* Gralloc::cloneBuffer(const hidl_handle& rawHandle,
                                            enum Tolerance /*tolerance*/) {
    const native_handle_t* bufferHandle = native_handle_clone(rawHandle.getNativeHandle());
    EXPECT_NE(nullptr, bufferHandle);

    if (bufferHandle) {
        mClonedBuffers.insert(bufferHandle);
    }

    return bufferHandle;
}

std::vector<const native_handle_t*> Gralloc::allocate(const BufferDescriptor& descriptor,
                                                      uint32_t count, bool import,
                                                      enum Tolerance tolerance,
                                                      uint32_t* outStride) {
    std::vector<const native_handle_t*> bufferHandles;
    bufferHandles.reserve(count);
    mAllocator->allocate(descriptor, count,
                         [&](const auto& tmpError, const auto& tmpStride, const auto& tmpBuffers) {
                             if (canTolerate(tolerance, tmpError)) {
                                 return;
                             }

                             ASSERT_EQ(Error::NONE, tmpError) << "failed to allocate buffers";
                             ASSERT_EQ(count, tmpBuffers.size()) << "invalid buffer array";

                             for (uint32_t i = 0; i < count; i++) {
                                 const native_handle_t* bufferHandle = nullptr;
                                 if (import) {
                                     ASSERT_NO_FATAL_FAILURE(
                                             bufferHandle = importBuffer(tmpBuffers[i], tolerance));
                                 } else {
                                     ASSERT_NO_FATAL_FAILURE(
                                             bufferHandle = cloneBuffer(tmpBuffers[i], tolerance));
                                 }
                                 if (bufferHandle) {
                                     bufferHandles.push_back(bufferHandle);
                                 }
                             }

                             if (outStride) {
                                 *outStride = tmpStride;
                             }
                         });

    if (::testing::Test::HasFatalFailure()) {
        bufferHandles.clear();
    }

    return bufferHandles;
}

const native_handle_t* Gralloc::allocate(const IMapper::BufferDescriptorInfo& descriptorInfo,
                                         bool import, enum Tolerance tolerance,
                                         uint32_t* outStride) {
    BufferDescriptor descriptor = createDescriptor(descriptorInfo);
    if (::testing::Test::HasFatalFailure()) {
        return nullptr;
    }

    auto buffers = allocate(descriptor, 1, import, tolerance, outStride);
    if (::testing::Test::HasFatalFailure()) {
        return nullptr;
    }

    if (buffers.size() != 1) {
        return nullptr;
    }
    return buffers[0];
}

sp<IMapper> Gralloc::getMapper() const {
    return mMapper;
}

BufferDescriptor Gralloc::createDescriptor(const IMapper::BufferDescriptorInfo& descriptorInfo) {
    BufferDescriptor descriptor;
    mMapper->createDescriptor(descriptorInfo, [&](const auto& tmpError, const auto& tmpDescriptor) {
        ASSERT_EQ(Error::NONE, tmpError) << "failed to create descriptor";
        descriptor = tmpDescriptor;
    });

    return descriptor;
}

const native_handle_t* Gralloc::importBuffer(const hidl_handle& rawHandle,
                                             enum Tolerance tolerance) {
    const native_handle_t* bufferHandle = nullptr;
    mMapper->importBuffer(rawHandle, [&](const auto& tmpError, const auto& tmpBuffer) {
        if (!canTolerate(tolerance, tmpError)) {
            ASSERT_EQ(Error::NONE, tmpError)
                    << "failed to import buffer %p" << rawHandle.getNativeHandle();
        }
        bufferHandle = static_cast<const native_handle_t*>(tmpBuffer);
    });

    if (bufferHandle) {
        mImportedBuffers.insert(bufferHandle);
    }

    return bufferHandle;
}

void Gralloc::freeBuffer(const native_handle_t* bufferHandle) {
    if (bufferHandle == nullptr) {
        return;
    }

    auto buffer = const_cast<native_handle_t*>(bufferHandle);

    if (mImportedBuffers.erase(bufferHandle)) {
        Error error = mMapper->freeBuffer(buffer);
        ASSERT_EQ(Error::NONE, error) << "failed to free buffer " << buffer;
    } else {
        mClonedBuffers.erase(bufferHandle);
        native_handle_close(buffer);
        native_handle_delete(buffer);
    }
}

void* Gralloc::lock(const native_handle_t* bufferHandle, uint64_t cpuUsage,
                    const IMapper::Rect& accessRegion, int acquireFence) {
    auto buffer = const_cast<native_handle_t*>(bufferHandle);

    NATIVE_HANDLE_DECLARE_STORAGE(acquireFenceStorage, 1, 0);
    hidl_handle acquireFenceHandle;
    if (acquireFence >= 0) {
        auto h = native_handle_init(acquireFenceStorage, 1, 0);
        h->data[0] = acquireFence;
        acquireFenceHandle = h;
    }

    void* data = nullptr;
    mMapper->lock(buffer, cpuUsage, accessRegion, acquireFenceHandle,
                  [&](const auto& tmpError, const auto& tmpData) {
                      ASSERT_EQ(Error::NONE, tmpError) << "failed to lock buffer " << buffer;
                      data = tmpData;
                  });

    if (acquireFence >= 0) {
        close(acquireFence);
    }

    return data;
}

int Gralloc::unlock(const native_handle_t* bufferHandle) {
    auto buffer = const_cast<native_handle_t*>(bufferHandle);

    int releaseFence = -1;
    mMapper->unlock(buffer, [&](const auto& tmpError, const auto& tmpReleaseFence) {
        ASSERT_EQ(Error::NONE, tmpError) << "failed to unlock buffer " << buffer;

        auto fenceHandle = tmpReleaseFence.getNativeHandle();
        if (fenceHandle) {
            ASSERT_EQ(0, fenceHandle->numInts) << "invalid fence handle " << fenceHandle;
            if (fenceHandle->numFds == 1) {
                releaseFence = dup(fenceHandle->data[0]);
                ASSERT_LT(0, releaseFence) << "failed to dup fence fd";
            } else {
                ASSERT_EQ(0, fenceHandle->numFds) << " invalid fence handle " << fenceHandle;
            }
        }
    });

    return releaseFence;
}

int Gralloc::flushLockedBuffer(const native_handle_t* bufferHandle) {
    auto buffer = const_cast<native_handle_t*>(bufferHandle);

    int releaseFence = -1;
    mMapper->flushLockedBuffer(buffer, [&](const auto& tmpError, const auto& tmpReleaseFence) {
        ASSERT_EQ(Error::NONE, tmpError) << "failed to flush locked buffer " << buffer;

        auto fenceHandle = tmpReleaseFence.getNativeHandle();
        if (fenceHandle) {
            ASSERT_EQ(0, fenceHandle->numInts) << "invalid fence handle " << fenceHandle;
            if (fenceHandle->numFds == 1) {
                releaseFence = dup(fenceHandle->data[0]);
                ASSERT_LT(0, releaseFence) << "failed to dup fence fd";
            } else {
                ASSERT_EQ(0, fenceHandle->numFds) << " invalid fence handle " << fenceHandle;
            }
        }
    });

    return releaseFence;
}

void Gralloc::rereadLockedBuffer(const native_handle_t* bufferHandle) {
    auto buffer = const_cast<native_handle_t*>(bufferHandle);

    ASSERT_EQ(Error::NONE, mMapper->rereadLockedBuffer(buffer));
}

bool Gralloc::validateBufferSize(const native_handle_t* bufferHandle,
                                 const IMapper::BufferDescriptorInfo& descriptorInfo,
                                 uint32_t stride) {
    auto buffer = const_cast<native_handle_t*>(bufferHandle);

    Error error = mMapper->validateBufferSize(buffer, descriptorInfo, stride);
    return error == Error::NONE;
}

void Gralloc::getTransportSize(const native_handle_t* bufferHandle, uint32_t* outNumFds,
                               uint32_t* outNumInts) {
    auto buffer = const_cast<native_handle_t*>(bufferHandle);

    *outNumFds = 0;
    *outNumInts = 0;
    mMapper->getTransportSize(buffer, [&](const auto& tmpError, const auto& tmpNumFds,
                                          const auto& tmpNumInts) {
        ASSERT_EQ(Error::NONE, tmpError) << "failed to get transport size";
        ASSERT_GE(bufferHandle->numFds, int(tmpNumFds)) << "invalid numFds " << tmpNumFds;
        ASSERT_GE(bufferHandle->numInts, int(tmpNumInts)) << "invalid numInts " << tmpNumInts;

        *outNumFds = tmpNumFds;
        *outNumInts = tmpNumInts;
    });
}

bool Gralloc::isSupported(const IMapper::BufferDescriptorInfo& descriptorInfo) {
    bool supported = false;
    mMapper->isSupported(descriptorInfo, [&](const auto& tmpError, const auto& tmpSupported) {
        ASSERT_EQ(Error::NONE, tmpError) << "failed to check is supported";
        supported = tmpSupported;
    });
    return supported;
}

bool Gralloc::isSupportedNoFailure(const IMapper::BufferDescriptorInfo& descriptorInfo) {
    bool supported = false;
    mMapper->isSupported(descriptorInfo, [&](const auto& tmpError, const auto& tmpSupported) {
        supported = tmpSupported && tmpError == Error::NONE;
    });
    return supported;
}

Error Gralloc::get(const native_handle_t* bufferHandle, const IMapper::MetadataType& metadataType,
                   hidl_vec<uint8_t>* outVec) {
    Error err;
    mMapper->get(const_cast<native_handle_t*>(bufferHandle), metadataType,
                 [&](const auto& tmpError, const hidl_vec<uint8_t>& tmpVec) {
                     err = tmpError;
                     *outVec = tmpVec;
                 });
    return err;
}

Error Gralloc::set(const native_handle_t* bufferHandle, const IMapper::MetadataType& metadataType,
                   const hidl_vec<uint8_t>& vec) {
    return mMapper->set(const_cast<native_handle_t*>(bufferHandle), metadataType, vec);
}

Error Gralloc::getFromBufferDescriptorInfo(const IMapper::BufferDescriptorInfo& descriptorInfo,
                                           const IMapper::MetadataType& metadataType,
                                           hidl_vec<uint8_t>* outVec) {
    Error err;
    mMapper->getFromBufferDescriptorInfo(
            descriptorInfo, metadataType,
            [&](const auto& tmpError, const hidl_vec<uint8_t>& tmpVec) {
                err = tmpError;
                *outVec = tmpVec;
            });
    return err;
}

Error Gralloc::getReservedRegion(const native_handle_t* bufferHandle, void** outReservedRegion,
                                 uint64_t* outReservedSize) {
    Error err;
    mMapper->getReservedRegion(
            const_cast<native_handle_t*>(bufferHandle),
            [&](const auto& tmpError, const auto& tmpReservedRegion, const auto& tmpReservedSize) {
                err = tmpError;
                *outReservedRegion = tmpReservedRegion;
                *outReservedSize = tmpReservedSize;
            });
    return err;
}

}  // namespace vts
}  // namespace V4_0
}  // namespace mapper
}  // namespace graphics
}  // namespace hardware
}  // namespace android
