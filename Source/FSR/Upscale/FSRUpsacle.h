# pragma once

#include "ffx_api.hpp"
#include "FSR/FSRTypes.h"
#include "Engine/Core/Types/BaseTypes.h"
#include "Engine/Core/Math/Vector2.h"
#include "Engine/Graphics/RenderTask.h"
#include "Engine/Core/Collections/Dictionary.h"

class GPUTexture;
class GPUContext;

// FSR upscaling integration.
API_CLASS(Namespace="AMD") class FSR_API FSRUpscale : public ScriptingObject
{
    DECLARE_SCRIPTING_TYPE(FSRUpscale);
public:
    /// <summary>
    /// Softening or sharpening factor to apply during the FSR pass. Negative values soften the image, positive values sharpen. In range [-1; 1].
    /// </summary>
    API_FIELD(Attributes="Range(-1.0f, 1.0f)") float Sharpness = 0.0f;

private:
    ffx::Context _ffxContext = nullptr;
    bool _debugView = false;
    Dictionary<String, uint64_t> _upscalerVersions;
    String _selectedUpscalerVersion;
    uint64_t _selectedUpscalerId = 0;
    FSRQuality _quality = FSRQuality::NativeAA;
    Int2 _contextSize = Int2::Zero;
    bool _contextReset = true;

public:
    /// <summary>
    /// Initializes FSR context
    /// </summary>
    bool Initialize(FSRSupport& support);

    /// <summary>
    /// Deinitializes FSR context.
    /// </summary>
    void Shutdown();

    /// <summary>
    /// Renders FSR upscaled image.
    /// </summary>
    void TemporalResolve(GPUContext* context, RenderContext& renderContext, GPUTexture* input, GPUTexture* output, const Float2& pixelOffset);

    /// <summary>
    /// Gets the available upscaler versions.
    /// </summary>
    API_PROPERTY() Array<String> GetUpscalerVersions() const;

    /// <summary>
    /// Gets the selected upscaler version.
    /// </summary>
    API_PROPERTY() StringView GetUpscalerVersion() const;

    /// <summary>
    /// Sets the upscaler version.
    /// </summary>
    API_PROPERTY() void SetUpscalerVersion(StringView newVersion);

    /// <summary>
    /// Gets the current Quality.
    /// </summary>
    API_PROPERTY() FSRQuality GetQuality() const;

    /// <summary>
    /// Sets the render scale that depend on FSR quality preset.
    /// </summary>
    API_PROPERTY() void SetQuality(FSRQuality quality);

    /// <summary>
    /// Sets the debug FSR view.
    /// </summary>
    API_FUNCTION() void SetDebugView(bool debugEnabled);

    /// <summary>
    /// Get render scale ratio from selected Quality mode.
    /// </summary>
    API_FUNCTION() float GetUpscaleRatioFromQuality(FSRQuality quality);

private:
    void DestroyContext();
    void UpdateFSRContext(const Int2& upscaleSize);
    void FillUpscalerVersions();
    static void ffxDebugMessage(uint32_t type, const wchar_t* message);
};