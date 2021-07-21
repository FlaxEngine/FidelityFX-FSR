using System;
using System.Runtime.InteropServices;
using FlaxEngine;

namespace FidelityFX
{
    /// <summary>
    /// FidelityFX Super Resolution effect renderer.
    /// </summary>
    public sealed class FSRPostFx : PostProcessEffect
    {
        [StructLayout(LayoutKind.Sequential)]
        private struct Data
        {
            public Vector4 Const0;
            public Vector4 Const1;
            public Vector4 Const2;
            public Vector4 Const3;
        }

        private readonly Shader _shader = Content.LoadAsync<Shader>(FSR.ShaderId);

        /// <inheritdoc />
        public override PostProcessEffectLocation Location => PostProcessEffectLocation.CustomUpscale;

        /// <inheritdoc />
        public override bool CanRender => !Input.GetKey(KeyboardKeys.J) && _shader && _shader.IsLoaded && base.CanRender;

        /// <inheritdoc />
        public override unsafe void Render(GPUContext context, ref RenderContext renderContext, GPUTexture input, GPUTexture output)
        {
            Profiler.BeginEventGPU("FSR");

            var outputView = renderContext.Task.OutputView;
            var outputViewport = renderContext.Task.OutputViewport;
            var inputSize = input.Size;
            var outputSize = outputViewport.Size;
            var outputWidth = (int)outputViewport.Width;
            var outputHeight = (int)outputViewport.Height;
            var desc = GPUTextureDescription.New2D(outputWidth, outputHeight, output.Format, GPUTextureFlags.ShaderResource | GPUTextureFlags.UnorderedAccess);
            var upscaled = RenderTargetPool.Get(ref desc);
            var sharpened = RenderTargetPool.Get(ref desc);
            var cb = _shader.GPU.GetCB(0);

            // Upscale pass
            if (cb != IntPtr.Zero)
            {
                FsrEasuCon(
                    out var data,
                    // Current frame render resolution
                    inputSize.X, inputSize.Y,
                    // Input texture resolution
                    inputSize.X, inputSize.Y,
                    // Upscaled output resolution
                    outputSize.X, outputSize.Y
                );
                context.UpdateCB(cb, new IntPtr(&data));
            }

            context.BindCB(0, cb);
            context.BindSR(0, input);
            context.BindUA(0, upscaled.View());
            context.Dispatch(_shader.GPU.GetCS("CS_Upscale"), (uint)(outputWidth + 15) / 16, (uint)(outputHeight + 15) / 16, 1);
            context.BindUA(0, null);
            context.FlushState();

            // Sharpen pass
            if (cb != IntPtr.Zero)
            {
                FsrRcasCon(
                    out var data
                );
                context.UpdateCB(cb, new IntPtr(&data));
            }

            context.BindSR(0, upscaled);
            context.BindUA(0, sharpened.View());
            context.Dispatch(_shader.GPU.GetCS("CS_Sharpen"), (uint)(outputWidth + 15) / 16, (uint)(outputHeight + 15) / 16, 1);
            context.BindUA(0, null);
            context.FlushState();

            // Copy pass
            context.SetViewportAndScissors(outputViewport);
            context.SetRenderTarget(outputView);
            context.Draw(sharpened);

            // TODO: disable film grain and chromatic aberration from PostProcessing and apply if manually after FSR

            RenderTargetPool.Release(upscaled);
            RenderTargetPool.Release(sharpened);

            Profiler.EndEventGPU();
        }

        // Reference: ffx_fsr1.h
        // inputViewportInPixels - This the rendered image resolution being upscaled
        // inputSizeInPixels - This is the resolution of the resource containing the input image (useful for dynamic resolution)
        // outputSizeInPixels - This is the display resolution which the input image gets upscaled to
        private static void FsrEasuCon(out Data data, float inputViewportInPixelsX, float inputViewportInPixelsY, float inputSizeInPixelsX, float inputSizeInPixelsY, float outputSizeInPixelsX, float outputSizeInPixelsY)
        {
            data.Const0.X = inputViewportInPixelsX * (1.0f / outputSizeInPixelsX);
            data.Const0.Y = inputViewportInPixelsY * (1.0f / outputSizeInPixelsY);
            data.Const0.Z = (0.5f) * inputViewportInPixelsX * (1.0f / outputSizeInPixelsX) - (0.5f);
            data.Const0.W = (0.5f) * inputViewportInPixelsY * (1.0f / outputSizeInPixelsY) - (0.5f);
            data.Const1.X = (1.0f / inputSizeInPixelsX);
            data.Const1.Y = (1.0f / inputSizeInPixelsY);
            data.Const1.Z = (1.0f) * (1.0f / inputSizeInPixelsX);
            data.Const1.W = (-1.0f) * (1.0f / inputSizeInPixelsY);
            data.Const2.X = (-1.0f) * (1.0f / inputSizeInPixelsX);
            data.Const2.Y = (2.0f) * (1.0f / inputSizeInPixelsY);
            data.Const2.Z = (1.0f) * (1.0f / inputSizeInPixelsX);
            data.Const2.W = (2.0f) * (1.0f / inputSizeInPixelsY);
            data.Const3.X = (0.0f) * (1.0f / inputSizeInPixelsX);
            data.Const3.Y = (4.0f) * (1.0f / inputSizeInPixelsY);
            data.Const3.Z = data.Const3.W = 0;
        }

        // Reference: ffx_fsr1.h
        private static void FsrRcasCon(out Data data)
        {
            // Hardcoded for sharpness = 1
            data.Const0.X = 0.5f;
            data.Const0.Y = 3.05697322e-05f;
            data.Const0.Z = 0;
            data.Const0.W = 0;
            data.Const1 = Vector4.Zero;
            data.Const2 = Vector4.Zero;
            data.Const3 = Vector4.Zero;
        }
    }
}
