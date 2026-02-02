export module Render.Image;
import Vendor.GraphicsAPI;
import "stb_image.h";
import Core.Prelude;
import Core.Coroutine;

import Core.Utilities;

import Render.CommandListSubmissionContext;

namespace
Engine {
    export struct GPUImageDescriptor {
        uint32_t width{};
        uint32_t height{};
        std::span<const uint8_t> imageData{};
        std::optional<uint32_t> rowPitchInBytes = std::nullopt;
        std::string_view debugName = "SimpleGPUImage";
        nvrhi::Format format = nvrhi::Format::RGBA8_UNORM;
        nvrhi::ResourceStates initialState = nvrhi::ResourceStates::ShaderResource;
        bool isRenderTarget = false;
        bool isUAV = false;
        bool keepInitialState = true;
    };

    export std::vector<nvrhi::TextureHandle> UploadImagesToGPU(
        std::span<const GPUImageDescriptor> descriptors,
        const Ref<CommandListSubmissionContext> &submissionContext) {
        std::vector<nvrhi::TextureHandle> textures;
        textures.reserve(descriptors.size());

        for (const auto &desc: descriptors) {
            nvrhi::TextureDesc textureDesc;
            textureDesc.width = desc.width;
            textureDesc.height = desc.height;
            textureDesc.format = desc.format;
            textureDesc.debugName = desc.debugName;
            textureDesc.isRenderTarget = desc.isRenderTarget;
            textureDesc.isUAV = desc.isUAV;
            textureDesc.initialState = desc.initialState;
            textureDesc.keepInitialState = desc.keepInitialState;

            textures.emplace_back(submissionContext->GetDevice()->createTexture(textureDesc));
        }

        auto commandList = submissionContext->GetDevice()->createCommandList();

        commandList->open();
        for (size_t i = 0; i < descriptors.size(); ++i) {
            uint32_t rowPitch = descriptors[i].rowPitchInBytes.has_value()
                                    ? descriptors[i].rowPitchInBytes.value()
                                    : descriptors[i].width * sizeof(uint32_t);
            commandList->writeTexture(textures[i], 0, 0,
                                      descriptors[i].imageData.data(),
                                      rowPitch);
        }
        commandList->close();

        submissionContext->SubmitTaskImmediate([&](CommandListSubmissionContext &ctx) {
            auto device = ctx.GetDevice();
            auto eventQuery = device->createEventQuery();

            size_t queryId = device->executeCommandList(commandList);

            device->setEventQuery(eventQuery, nvrhi::CommandQueue::Graphics, queryId);

            device->waitEventQuery(eventQuery);
        });


        return textures;
    }

    export nvrhi::TextureHandle UploadImageToGPU(
        const GPUImageDescriptor &descriptor,
        const Ref<CommandListSubmissionContext> &submissionContext) {
        auto results = UploadImagesToGPU(std::span{&descriptor, 1}, submissionContext);
        return std::move(results.front());
    }

    export class CPUImage {
        uint32_t mWidth{};
        uint32_t mHeight{};
        std::unique_ptr<IBuffer<uint8_t>> mBuffer;

    public:
        GPUImageDescriptor GetGPUDescriptor(std::string_view debugName = "CPUImage") const {
            return GPUImageDescriptor{
                .width = mWidth,
                .height = mHeight,
                .imageData = mBuffer->AsSpan(),
                .debugName = debugName
            };
        }

        uint32_t GetWidth() const {
            return mWidth;
        }

        uint32_t GetHeight() const {
            return mHeight;
        }

        const uint8_t *GetData() const {
            return mBuffer->Data();
        }

        CPUImage(uint32_t width, uint32_t height, std::unique_ptr<uint8_t[]> data)
            : mWidth(width), mHeight(height),
              mBuffer(std::make_unique<DynamicBuffer<uint8_t>>(data.release(),
                                                               width * height * 4,
                                                               ResourceOwnership::Transferred{})) {}

        CPUImage(uint32_t width, uint32_t height, std::unique_ptr<IBuffer<uint8_t>> buffer)
            : mWidth(width), mHeight(height),
              mBuffer(std::move(buffer)) {}

        CPUImage(uint32_t width, uint32_t height, ResourceState::Uninitialized)
            : mWidth(width), mHeight(height),
              mBuffer(std::make_unique<DynamicBuffer<uint8_t>>(width * height * 4,
                                                               ResourceState::Uninitialized{})) {}

        CPUImage(ResourceState::Uninitialized)
            : mBuffer(nullptr) {}
    };

    export Ref<CPUImage> LoadImageFromFile(const std::filesystem::path &filePath) {
        int width, height, channels;
        stbi_uc *imgData = stbi_load(filePath.string().c_str(), &width, &height, &channels, 4);
        if (!imgData) {
            throw RuntimeException("Failed to load image: " + filePath.string());
        }

        struct Deleter {
            void operator()(stbi_uc *ptr) const {
                stbi_image_free(ptr);
            }
        };

        DynamicBuffer<uint8_t, Deleter> buffer(
            imgData,
            static_cast<size_t>(width) * static_cast<size_t>(height) * 4,
            ResourceOwnership::Transferred{}
        );

        return MakeRef<CPUImage>(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                 std::make_unique<DynamicBuffer<uint8_t, Deleter>>(std::move(buffer)));
    }

    export Awaitable<Ref<CPUImage>> LoadImageFromFileAsync(const std::filesystem::path &filePath) {
        co_return LoadImageFromFile(filePath);
    }
}
