#include "FSRUpsacle.h"
#include "Engine/Core/Log.h"
#include "Engine/Engine/Time.h"
#include "Engine/Graphics/GPUDevice.h"
#include "Engine/Graphics/GPUContext.h"
#include "Engine/Level/Actors/Camera.h"
#include "Engine/Graphics/Graphics.h"
#include "Engine/Graphics/RenderBuffers.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Graphics/Textures/GPUTexture.h"
#include "Engine/Scripting/Enums.h"
#include <ffx_api.hpp>
#include <ffx_api_types.h>
#include <ffx_upscale.hpp>
#include <dx12/ffx_api_dx12.hpp>

FSRUpscale::FSRUpscale(const SpawnParams& params) 
    : ScriptingObject(params)
{
}

bool FSRUpscale::Initialize(FSRSupport& support)
{
#if !PLATFORM_WINDOWS
    support = FSRSupport::NotSupportedPlatform;
    return true;
#endif

    switch (GPUDevice::Instance->GetRendererType())
    {
    case RendererType::DirectX12:
    {
        // Initial context size, it gets recreated on the first frame if the real output size differs
        const auto task = MainRenderTask::Instance;
        const Viewport viewport = task ? task->GetOutputViewport() : Viewport(0, 0, 0, 0);
        UpdateFSRContext(viewport.Width >= 16 && viewport.Height >= 16 ? Int2((int32)viewport.Width, (int32)viewport.Height) : Int2(1920, 1080));
        break;
    }
#if GRAPHICS_API_VULKAN
    case RendererType::Vulkan:
        // Waiting until AMD finally add support for Vulkan...
        support = FSRSupport::NotSupportedRenderingBackend;
        return true;
#endif
    default:
        LOG(Warning, "[FSR] FSR requires DirectX 12, current renderer is {}. Start with -d3d12 to use it.", ScriptingEnum::ToString(GPUDevice::Instance->GetRendererType()));
        support = FSRSupport::NotSupportedRenderingBackend;
        return true;
    }

    support = FSRSupport::Supported;
    FillUpscalerVersions();
    return false;
}

void FSRUpscale::Shutdown()
{
    if (!_ffxContext)
        return;
    DestroyContext();
    _upscalerVersions.Clear();
}

void FSRUpscale::TemporalResolve(GPUContext* context, RenderContext& renderContext, GPUTexture* input,
                                 GPUTexture* output, const Float2& pixelOffset)
{
    //TODO noise on NativeAA
    const auto renderSize = input->Size();
    const auto upscaleSize = output->Size();

    // FSR internal resources (history, locks, etc.) are allocated at context creation using maxUpscaleSize/maxRenderSize.
    // If the output is bigger than that, the parts outside the allocated area (right/bottom) become garbage.
    // Recreate the context whenever the output size changes (eg. window resize or different render task).
    if (!_ffxContext || upscaleSize != _contextSize)
    {
        UpdateFSRContext(upscaleSize);
        if (!_ffxContext)
            return;
    }
    const float sharpness = Math::Clamp(Sharpness, -1.0f, 1.0f);
    ffx::ReturnCode retCode;

    // Since fsr only on D3D12
    context->SetResourceState(output, 0x8); // D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    context->SetResourceState(input, 0x40 | 0x80); // D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    context->SetResourceState(renderContext.Task->Buffers->DepthBuffer, 0x40 | 0x80); // D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    if (renderContext.Task->Buffers->MotionVectors)
        context->SetResourceState(renderContext.Task->Buffers->MotionVectors, 0x40 | 0x80); // D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE

    // Flax only queues the barriers above, submit them to the command list before FSR records its own work.
    // Otherwise FSR reads the inputs while the GPU is still rendering them (unfinished right/bottom parts of the frame).
    context->FlushState();

    ffx::DispatchDescUpscale upscale{};
    upscale.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
    upscale.commandList = (ID3D12GraphicsCommandList*)context->GetNativePtr();

    // ffx::QueryDescUpscaleGetJitterOffset
    upscale.color = ffxApiGetResourceDX12((ID3D12Resource*)input->GetNativePtr(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    upscale.depth = ffxApiGetResourceDX12((ID3D12Resource*)renderContext.Task->Buffers->DepthBuffer->GetNativePtr(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    upscale.motionVectors = ffxApiGetResourceDX12((ID3D12Resource*)(renderContext.Task->Buffers->MotionVectors ? renderContext.Task->Buffers->MotionVectors->GetNativePtr() : nullptr), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    upscale.output = ffxApiGetResourceDX12((ID3D12Resource*)output->GetNativePtr(), FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);

    upscale.jitterOffset = { pixelOffset.X, pixelOffset.Y };
    // Flax motion vectors are in UV space as (CurrentUV - PreviousUV), FSR expects pixels as (Previous - Current)
    upscale.motionVectorScale = { -static_cast<float>(renderSize.X), -static_cast<float>(renderSize.Y) };
    upscale.renderSize = { static_cast<uint32_t>(renderSize.X), static_cast<uint32_t>(renderSize.Y) };
    upscale.upscaleSize = { static_cast<uint32_t>(upscaleSize.X), static_cast<uint32_t>(upscaleSize.Y) };

    upscale.enableSharpening = true;
    upscale.sharpness = sharpness;
    upscale.frameTimeDelta = static_cast<float>(Time::Draw.UnscaledDeltaTime.GetTotalMilliseconds());
    upscale.preExposure = 1.0f;
    upscale.reset = renderContext.Task->IsCameraCut || _contextReset;
    _contextReset = false;
    upscale.cameraNear = renderContext.View.Near;
    upscale.cameraFar = renderContext.View.Far;
    upscale.cameraFovAngleVertical = 2.0f * Math::Atan(1.0f / renderContext.View.Projection.M22);
    upscale.viewSpaceToMetersFactor = 1.0f;
    if (Graphics::GammaColorSpace) upscale.flags |= FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB;
#if BUILD_DEVELOPMENT || BUILD_DEBUG
    if (_debugView) upscale.flags |= FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW;
#endif
    retCode = ffx::Dispatch(_ffxContext, upscale);
    if (retCode != ffx::ReturnCode::Ok)
    {
        LOG(Error, "[FSR] FSR is failed! Something gone wrong");
        return;
    }
    context->ForceRebindDescriptors();
    context->ResetState();
}

Array<String> FSRUpscale::GetUpscalerVersions() const
{
    Array<String> result;
    _upscalerVersions.GetKeys(result);
    return result;
}

StringView FSRUpscale::GetUpscalerVersion() const
{
    return _selectedUpscalerVersion;
}

void FSRUpscale::SetUpscalerVersion(StringView newVersion)
{
    const auto newUpscalerId = _upscalerVersions.TryGet(newVersion);
    if (!newUpscalerId)
    {
        LOG(Error, "[FSR] Cannot find upscaler version of {}", newVersion);
        return;
    }
    _selectedUpscalerVersion = newVersion;
    _selectedUpscalerId = *newUpscalerId;
    if (_contextSize.X > 0 && _contextSize.Y > 0)
        UpdateFSRContext(_contextSize);
}

FSRQuality FSRUpscale::GetQuality() const
{
    return _quality;
}

void FSRUpscale::SetQuality(FSRQuality quality)
{
    const float selectedRatio = GetUpscaleRatioFromQuality(quality);
    if (selectedRatio == 0.0f) return;
    
    const auto task = MainRenderTask::Instance;
    if (!task) return;
     
    LOG(Info, "[FSR] Selected ratio from quality mode: {} ({} %)", selectedRatio, (int32)(100.0f / selectedRatio));
    task->RenderScale = 1.0f / selectedRatio;
    _quality = quality;
}

void FSRUpscale::SetDebugView(bool debugEnabled)
{
    _debugView = debugEnabled;
}

float FSRUpscale::GetUpscaleRatioFromQuality(FSRQuality quality)
{
    float outRatio;
    ffx::QueryDescUpscaleGetUpscaleRatioFromQualityMode getResolutionRatio;
    getResolutionRatio.header.type = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETUPSCALERATIOFROMQUALITYMODE;
    getResolutionRatio.qualityMode = (uint32_t)quality;
    getResolutionRatio.pOutUpscaleRatio = &outRatio;
    auto result = ffx::Query(getResolutionRatio);
    if (result != ffx::ReturnCode::Ok)
    {
        LOG(Error, "[FSR] Failed to get upscale ration with this {} quality mode!", (uint32_t)quality);
        return 0.0f;
    }
    return outRatio;
}

void FSRUpscale::DestroyContext()
{
    if (_ffxContext)
    {
        // Old context resources might still be used by in-flight frames
        GPUDevice::Instance->WaitForGPU();
        ffx::DestroyContext(_ffxContext);
        _ffxContext = nullptr;
    }
    _contextSize = Int2::Zero;
}

void FSRUpscale::UpdateFSRContext(const Int2& upscaleSize)
{
    DestroyContext();
    _contextSize = upscaleSize;
    _contextReset = true;

    ffx::CreateBackendDX12Desc createBackend {};
    createBackend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
    createBackend.device = static_cast<ID3D12Device*>(GPUDevice::Instance->GetNativePtr());

    ffx::CreateContextDescUpscale createUpscale {};
    createUpscale.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
    createUpscale.flags = FFX_UPSCALE_ENABLE_AUTO_EXPOSURE | FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION | FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE;
#if REVERSE_Z
    createUpscale.flags |= FFX_UPSCALE_ENABLE_DEPTH_INVERTED;
#endif
#if !BUILD_RELEASE
    createUpscale.flags |= FFX_UPSCALE_ENABLE_DEBUG_CHECKING | FFX_UPSCALE_ENABLE_DEBUG_VISUALIZATION;
    createUpscale.fpMessage = &FSRUpscale::ffxDebugMessage;
#endif
    // Render resolution is never higher than the output (render scale <= 1), so output size is the max for both
    createUpscale.maxRenderSize = FfxApiDimensions2D{static_cast<uint32_t>(upscaleSize.X), static_cast<uint32_t>(upscaleSize.Y)};
    createUpscale.maxUpscaleSize = FfxApiDimensions2D{static_cast<uint32_t>(upscaleSize.X), static_cast<uint32_t>(upscaleSize.Y)};

    ffx::CreateContextDescOverrideVersion versionOverride {};
    versionOverride.header.type = FFX_API_DESC_TYPE_OVERRIDE_VERSION;
    ffx::ReturnCode returnCode;
    if (_selectedUpscalerVersion != String::Empty)
    {
        versionOverride.versionId = _selectedUpscalerId;
        returnCode = ffx::CreateContext(_ffxContext, nullptr, createUpscale, createBackend, versionOverride);
    }
    else
    {
        returnCode = ffx::CreateContext(_ffxContext, nullptr, createUpscale, createBackend);
    }
    if (returnCode != ffx::ReturnCode::Ok)
    {
        LOG(Error, "[FSR] ffx create context failed");
        _ffxContext = nullptr;
        _contextSize = Int2::Zero;
    }
}

void FSRUpscale::FillUpscalerVersions()
{
    auto result = ffx::ReturnCode::Error;
    auto gpuDevice = GPUDevice::Instance;
    uint64_t versionCount;

    ffx::QueryDescGetVersions getAvailableVersions = {};
    getAvailableVersions.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
    getAvailableVersions.createDescType = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION;
    getAvailableVersions.device = static_cast<ID3D12Device*>(gpuDevice->GetNativePtr());
    getAvailableVersions.outputCount = &versionCount;

    // First query: get count
    result = ffx::Query(getAvailableVersions);
    if (result == ffx::ReturnCode::Ok)
    {
        LOG(Info, "[FSR] ffx Query get versionCount is success: versionCount is {}", versionCount);
    }
    else
    {
        LOG(Error, "[FSR] ffx Query get versionCount failed");
        return;
    }

    // Allocate storage
    _upscalerVersions.Clear();
    Array<uint64_t> versionIds;
    Array<const char*> versionNames;
    versionIds.Resize((int32)versionCount);
    versionNames.Resize((int32)versionCount);
    getAvailableVersions.outputCount = &versionCount;
    getAvailableVersions.versionIds = versionIds.Get();
    getAvailableVersions.versionNames = versionNames.Get();

    // Second query: fill arrays
    result = ffx::Query(getAvailableVersions);
    if (result == ffx::ReturnCode::Ok)
    {
        LOG(Info, "[FSR] ffx Query filled arrays, versionIds length is {}, versionNames length is {}", versionIds.Count(), versionNames.Count());
    }
    else
    {
        LOG(Error, "[FSR] ffx Query cannot filled arrays");
        return;
    }
    for (int32 i = 0; i < versionCount; i++)
    {
        LOG(Info, "[FSR] The versionIds is {}, versionNames is {}", versionIds[i], String(versionNames[i]));
    	_upscalerVersions.Add(String(versionNames[i]), versionIds[i]);
    }

    ffxQueryGetProviderVersion getVersion = {0};
    getVersion.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
    result = ffx::Query(_ffxContext, getVersion);
    LOG(Info, "[FSR] The current Upscaler versionId is: {}, versionName is {}", getVersion.versionId, String(getVersion.versionName));
    _selectedUpscalerVersion = String(getVersion.versionName);
    _selectedUpscalerId = getVersion.versionId;
}

void FSRUpscale::ffxDebugMessage(uint32_t type, const wchar_t* message)
{
    LOG(Warning, "[FSR] Message from ffxUpscale type: {}, message: {}", type, String(message));
}
