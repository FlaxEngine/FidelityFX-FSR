#include "./Flax/Common.hlsl"

#define A_GPU 1
#define A_HLSL 1
#include "./FidelityFX-FSR/ffx_a.h"

META_CB_BEGIN(0, Data)
float2 InputViewportInPixels;
float2 InputSizeInPixels;
float2 OutputSizeInPixels;
float Sharpness;
float Dummy;
META_CB_END

Texture2D<float4> Input : register(t0);
RWTexture2D<float4> Output : register(u0);

#if _CS_Upscale

AF4 FsrEasuRF(AF2 p) { AF4 res = Input.GatherRed(SamplerLinearClamp, p, int2(0, 0)); return res; }
AF4 FsrEasuGF(AF2 p) { AF4 res = Input.GatherGreen(SamplerLinearClamp, p, int2(0, 0)); return res; }
AF4 FsrEasuBF(AF2 p) { AF4 res = Input.GatherBlue(SamplerLinearClamp, p, int2(0, 0)); return res; }
#define FSR_EASU_F 1

#include "./FidelityFX-FSR/ffx_fsr1.h"

void CurrFilter(int2 pos, AU4 const0, AU4 const1, AU4 const2, AU4 const3)
{
	AF3 c;
	FsrEasuF(c, pos, const0, const1, const2, const3);
	Output[pos] = float4(c, 1);
}

// Compute Shader for FSR Upscaling pass
META_CS(true, FEATURE_LEVEL_SM5)
[numthreads(64, 1, 1)]
void CS_Upscale(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
	// Initialize constants
	AU4 const0, const1, const2, const3;
	FsrEasuCon(
		const0, const1, const2, const3,
		InputViewportInPixels.x, InputViewportInPixels.y,
		InputSizeInPixels.x, InputSizeInPixels.y,
		OutputSizeInPixels.x, OutputSizeInPixels.y
	);

	// Do remapping of local xy in workgroup for a more PS-like swizzle pattern
	AU2 gxy = ARmp8x8(groupThreadID.x) + AU2(groupID.x << 4u, groupID.y << 4u);
	CurrFilter(gxy, const0, const1, const2, const3);
	gxy.x += 8u;
	CurrFilter(gxy, const0, const1, const2, const3);
	gxy.y += 8u;
	CurrFilter(gxy, const0, const1, const2, const3);
	gxy.x -= 8u;
	CurrFilter(gxy, const0, const1, const2, const3);
}

#endif

#if _CS_Sharpen

#define FSR_RCAS_F
AF4 FsrRcasLoadF(ASU2 p) { return Input.Load(int3(ASU2(p), 0)); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) { }

#include "./FidelityFX-FSR/ffx_fsr1.h"

void CurrFilter(int2 pos, AU4 const0)
{
	AF3 c;
	FsrRcasF(c.r, c.g, c.b, pos, const0);
	Output[pos] = float4(c, 1);
}

// Compute Shader for FSR Sharpen pass
META_CS(true, FEATURE_LEVEL_SM5)
[numthreads(64, 1, 1)]
void CS_Sharpen(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
	// Initialize constants
	AU4 const0;
	FsrRcasCon(const0, Sharpness);

	// Do remapping of local xy in workgroup for a more PS-like swizzle pattern
	AU2 gxy = ARmp8x8(groupThreadID.x) + AU2(groupID.x << 4u, groupID.y << 4u);
	CurrFilter(gxy, const0);
	gxy.x += 8u;
	CurrFilter(gxy, const0);
	gxy.y += 8u;
	CurrFilter(gxy, const0);
	gxy.x -= 8u;
	CurrFilter(gxy, const0);
}

#endif
