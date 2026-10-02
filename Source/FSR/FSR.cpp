#include "FSR.h"
#include "FSRPostFx.h"
#include "Engine/Core/Log.h"
#include "Engine/Graphics/RenderTask.h"
#include "Engine/Scripting/SoftTypeReference.h"
#include "Engine/Scripting/Plugins/PluginManager.h"
#include "Upscale/FSRUpsacle.h"

FSR::FSR(const SpawnParams& params)
    : GamePlugin(params)
{
}

FSRSupport FSR::GetSupport() const
{
    return _support;
}

FSR* FSR::GetInstance()
{
    return PluginManager::GetPlugin<FSR>();
}

FSRUpscale* FSR::GetUpscale() const
{
    return _fsrUpscale;
}

void FSR::ApplyUpscaler(FSRQuality quality)
{
    if (PostFX) return;

    PostFX = New<FSRPostFx>();
    SceneRenderTask::AddGlobalCustomPostFx(PostFX);
    _fsrUpscale->SetQuality(quality);
}

void FSR::RemoveUpscaler()
{
    if (!PostFX) return;

    SceneRenderTask::RemoveGlobalCustomPostFx(PostFX);
    PostFX->DeleteObject();
    PostFX = nullptr;

    const auto task = MainRenderTask::Instance;
    if (task && task->RenderScale != 1.0f)
    {
		LOG(Info, "[FSR] Resetting render scale back to 1.0 (100 %)");
		task->RenderScale = 1.0f;
    }
}

void FSR::Initialize()
{
    _fsrUpscale = NewObject<FSRUpscale>();
    _fsrUpscale->Initialize(_support);
}

void FSR::Deinitialize()
{
    RemoveUpscaler();
    _fsrUpscale->Shutdown();
    _fsrUpscale->DeleteObject();
}

