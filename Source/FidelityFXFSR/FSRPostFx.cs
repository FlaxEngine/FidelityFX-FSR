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
            public Vector2 InputViewportInPixels;
            public Vector2 InputSizeInPixels;
            public Vector2 OutputSizeInPixels;
            public float Sharpness;
            public float Dummy;
        }

        private readonly Shader _shader = Content.LoadAsync<Shader>(FSR.ShaderId);

        /// <summary>
        /// The sharpening pass value in range 0-2, where 0 = maximum sharpness.
        /// </summary>
        public float Sharpness = 0.25f;

        /// <inheritdoc />
        public override PostProcessEffectLocation Location => PostProcessEffectLocation.CustomUpscale;

        /// <inheritdoc />
        public override bool CanRender => _shader && _shader.IsLoaded && base.CanRender;

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
            var desc = GPUTextureDescription.New2D(outputWidth, outputHeight, PixelFormat.R8G8B8A8_UNorm, GPUTextureFlags.ShaderResource | GPUTextureFlags.UnorderedAccess);
            var upscaled = RenderTargetPool.Get(ref desc);
            var sharpened = RenderTargetPool.Get(ref desc);
            var cb = _shader.GPU.GetCB(0);
            if (cb != IntPtr.Zero)
            {
                var data = new Data
                {
                    InputViewportInPixels = inputSize,
                    InputSizeInPixels = inputSize,
                    OutputSizeInPixels = outputSize,
                    Sharpness = Sharpness,
                };
                context.UpdateCB(cb, new IntPtr(&data));
            }

            context.BindCB(0, cb);
            context.BindSR(0, input);
            context.BindUA(0, upscaled.View());
            context.Dispatch(_shader.GPU.GetCS("CS_Upscale"), (uint)(outputWidth + 15) / 16, (uint)(outputHeight + 15) / 16, 1);
            context.ResetUA();

            context.BindSR(0, upscaled);
            context.BindUA(0, sharpened.View());
            context.Dispatch(_shader.GPU.GetCS("CS_Sharpen"), (uint)(outputWidth + 15) / 16, (uint)(outputHeight + 15) / 16, 1);
            context.ResetUA();

            // Copy pass
            context.SetViewportAndScissors(outputViewport);
            context.SetRenderTarget(outputView);
            context.Draw(sharpened);

            // TODO: disable film grain and chromatic aberration from PostProcessing and apply if manually after FSR

            RenderTargetPool.Release(upscaled);
            RenderTargetPool.Release(sharpened);

            Profiler.EndEventGPU();
        }
    }
}
