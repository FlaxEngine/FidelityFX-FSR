#pragma once

#include "Engine/Core/Types/BaseTypes.h"

/// <summary>
/// FSR support information.
/// </summary>
API_ENUM(Namespace="AMD") enum class FSRSupport
{
	// FSR is supported
	Supported,

	// FSR is not supported due to incompatible platform
	NotSupportedPlatform,

	// FSR is not supported due to incompatible rendering backend
	NotSupportedRenderingBackend,

	MAX
};

/// <summary>
/// FSR quality modes.
/// </summary>
API_ENUM(Namespace="AMD") enum class FSRQuality
{
	// Native resolution.
	NativeAA = 0,
	// Quality mode.
	Quality = 1,
	// Balanced mode.
	Balanced = 2,
	// Performance mode.
	Performance = 3,
	// Ultra Performance mode.
	UltraPerformance = 4,
};
