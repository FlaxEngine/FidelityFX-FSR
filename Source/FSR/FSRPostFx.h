#pragma once

#include "Engine/Graphics/PostProcessEffect.h"

// FSR rendering effect.
API_CLASS() class FSR_API FSRPostFx : public PostProcessEffect
{
	DECLARE_SCRIPTING_TYPE(FSRPostFx);
public:
    // [PostProcessEffect]
    bool CanRender(const RenderContext& renderContext) const override;
    void PreRender(GPUContext* context, RenderContext& renderContext) override;
    void Render(GPUContext* context, RenderContext& renderContext, GPUTexture* input, GPUTexture* output) override;
};
