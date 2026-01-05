export module Render.Image;
import Vendor.GraphicsAPI;
import "stb_image.h";
import Core.Prelude;

namespace
Engine {
    export struct SimpleGPUImageDescriptor {
        uint32_t width{};
        uint32_t height{};
        std::span<const uint32_t> imageData{};
        std::optional<uint32_t> rowPitchInBytes = std::nullopt;
        std::string_view debugName = "SimpleGPUImage";
        nvrhi::Format format = nvrhi::Format::RGBA8_UNORM;
        nvrhi::ResourceStates initialState = nvrhi::ResourceStates::ShaderResource;
        bool isRenderTarget = false;
        bool isUAV = false;
        bool keepInitialState = true;
    };

    export std::vector<nvrhi::TextureHandle> UploadImagesToGPU(
        std::span<const SimpleGPUImageDescriptor> descriptors,
        const nvrhi::DeviceHandle &device,
        const nvrhi::CommandListHandle &commandList) {
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

            textures.emplace_back(device->createTexture(textureDesc));
        }

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

        auto eventQuery = device->createEventQuery();

        device->executeCommandList(commandList);

        device->setEventQuery(eventQuery, nvrhi::CommandQueue::Graphics);

        device->waitEventQuery(eventQuery);

        return textures;
    }

    export nvrhi::TextureHandle UploadImageToGPU(
        const SimpleGPUImageDescriptor &descriptor,
        const nvrhi::DeviceHandle &device,
        const nvrhi::CommandListHandle &commandList) {
        auto results = UploadImagesToGPU(std::span{&descriptor, 1}, device, commandList);
        return std::move(results.front());
    }

    export struct CPUSimpleImage {
        uint32_t width{};
        uint32_t height{};
        std::shared_ptr<uint32_t[]> data{};

        SimpleGPUImageDescriptor GetGPUDescriptor(std::string_view debugName = "CPUSimpleImage") const {
            return SimpleGPUImageDescriptor{
                .width = width,
                .height = height,
                .imageData = std::span<const uint32_t>(data.get(), width * height),
                .debugName = debugName
            };
        }
    };

    export CPUSimpleImage LoadImageFromFile(const std::filesystem::path& filePath) {
        int width, height, channels;
        stbi_uc* imgData = stbi_load(filePath.string().c_str(), &width, &height, &channels, 4);
        if (!imgData) {
            throw std::runtime_error("Failed to load image: " + filePath.string());
        }

        auto data = std::shared_ptr<uint32_t[]>(reinterpret_cast<uint32_t*>(imgData), [](uint32_t* p) {
            stbi_image_free(reinterpret_cast<stbi_uc*>(p));
        });

        return CPUSimpleImage{
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .data = data
        };
    }

    export std::future<CPUSimpleImage> LoadImageFromFileAsync(const std::filesystem::path& filePath) {
        return std::async(std::launch::async, [filePath]() {
            return LoadImageFromFile(filePath);
        });
    }
}
