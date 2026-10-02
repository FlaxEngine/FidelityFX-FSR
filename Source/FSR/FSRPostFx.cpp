#include "FSRPostFx.h"
#include "FSR.h"
#include "Engine/Graphics/GPUContext.h"
#include "Engine/Graphics/RenderTargetPool.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Graphics/Textures/GPUTexture.h"
#include "Engine/Profiler/Profiler.h"
#include "Engine/Renderer/RenderList.h"
#include "Upscale/FSRUpsacle.h"

FSRPostFx::FSRPostFx(const SpawnParams& params)
    : PostProcessEffect(params)
{
    Location = PostProcessEffectLocation::CustomUpscale;
}

bool FSRPostFx::CanRender(const RenderContext& renderContext) const
{
    const auto fsr = FSR::GetInstance();
    return PostProcessEffect::CanRender() && fsr && fsr->GetSupport() == FSRSupport::Supported;
}

void FSRPostFx::PreRender(GPUContext* context, RenderContext& renderContext)
{
    if (!CanRender(renderContext)) return;

    // Adjust render setup
    renderContext.List->Setup.UpscaleLocation = RenderingUpscaleLocation::BeforePostProcessing;
    renderContext.List->Setup.UseTemporalAAJitter = true;

    // Disable AA
    renderContext.List->Settings.AntiAliasing.Mode = AntialiasingMode::None;

    // Render Motion Vectors at full-res
    renderContext.List->Setup.UseMotionVectors = true;
    renderContext.List->Settings.MotionBlur.MotionVectorsResolution = ResolutionMode::Full;
}

void FSRPostFx::Render(GPUContext* context, RenderContext& renderContext, GPUTexture* input, GPUTexture* output)
{
    PROFILE_GPU_CPU("FSR");
    GPUTexture* fsrOutput = output;
    if (!fsrOutput->IsUnorderedAccess())
    {
        GPUTextureDescription desc = output->GetDescription();
        desc.Flags &= ~GPUTextureFlags::BackBuffer;
        desc.Flags |= GPUTextureFlags::UnorderedAccess;
        fsrOutput = RenderTargetPool::Get(desc);
    }

    const auto fsr = FSR::GetInstance();
    // Flax jitter is a clip-space offset (applied to projection), FSR wants it in render pixels (with Y pointing down)
    const Float2 pixelOffset(renderContext.View.TemporalAAJitter.X * 0.5f * (float)input->Width(), -renderContext.View.TemporalAAJitter.Y * 0.5f * (float)input->Height());
    fsr->_fsrUpscale->TemporalResolve(context, renderContext, input, fsrOutput, pixelOffset);
    if (fsrOutput != output)
    {
        PROFILE_GPU("Copy");
        context->CopyResource(output, fsrOutput);
        RenderTargetPool::Release(fsrOutput);
    }
}
