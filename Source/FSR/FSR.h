#pragma once

#include "Engine/Scripting/Plugins/GamePlugin.h"
#include "FSRTypes.h"

class FSRUpscale;
class TestScriptableObj;
class FSRPostFx;

/// <summary>
/// FSR plugin
/// </summary>
API_CLASS(Namespace="AMD") class FSR_API FSR : public GamePlugin
{
    friend FSRPostFx;
    DECLARE_SCRIPTING_TYPE(FSR);
public:
    /// <summary>
    /// FSR post process effect.
    /// </summary>
    API_FIELD(ReadOnly) FSRPostFx* PostFX = nullptr;

private:
    FSRUpscale* _fsrUpscale = nullptr;
    FSRSupport _support = FSRSupport::MAX;

public:
    /// <summary>
    /// Gets FSR support information.
    /// </summary>
    API_PROPERTY() FSRSupport GetSupport() const;

    /// <summary>
    /// Gets FSR plugin instance.
    /// </summary>
    API_PROPERTY() static FSR* GetInstance();

    /// <summary>
    /// Gets FSR upscale.
    /// </summary>
    API_PROPERTY() FSRUpscale* GetUpscale() const;

    /// <summary>
    /// Apply FSR upscaler postfx and sets the initial quality (and render scale for the main view).
    /// </summary>
    API_FUNCTION() void ApplyUpscaler(FSRQuality quality = FSRQuality::Balanced);

    /// <summary>
    /// Remove FSR upscaler postfx and sets the render scale back to 1.
    /// </summary>
    API_FUNCTION() void RemoveUpscaler();

public:
    // [GamePlugin]
    void Initialize() override;
    void Deinitialize() override;
};
