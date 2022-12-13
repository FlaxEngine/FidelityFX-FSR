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
        public FSRPostFx()
        {
            Location = PostProcessEffectLocation.CustomUpscale;
        }

        /// <inheritdoc />
        public override bool CanRender()
        {
            return _shader && _shader.IsLoaded && base.CanRender();
        }

        /// <inheritdoc />
        public override unsafe void Render(GPUContext context, ref RenderContext renderContext, GPUTexture input, GPUTexture output)
        {
            Profiler.BeginEventGPU("FSR");

            var inputSize = input.Size;
            var outputSize = output.Size;
            var outputWidth = output.Width;
            var outputHeight = output.Height;
            var desc = GPUTextureDescription.New2D(outputWidth, outputHeight, output.Format, GPUTextureFlags.ShaderResource | GPUTextureFlags.UnorderedAccess);
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

            // Upscale
            context.BindCB(0, cb);
            context.BindSR(0, input);
            context.BindUA(0, upscaled.View());
            context.Dispatch(_shader.GPU.GetCS("CS_Upscale"), (uint)(outputWidth + 15) / 16, (uint)(outputHeight + 15) / 16, 1);
            context.ResetUA();

            // Sharpen
            context.BindSR(0, upscaled);
            context.BindUA(0, sharpened.View());
            context.Dispatch(_shader.GPU.GetCS("CS_Sharpen"), (uint)(outputWidth + 15) / 16, (uint)(outputHeight + 15) / 16, 1);
            context.ResetUA();

            // Copy pass
            context.SetViewportAndScissors(outputWidth, outputHeight);
            context.SetRenderTarget(output.View());
            context.Draw(sharpened);

            RenderTargetPool.Release(upscaled);
            RenderTargetPool.Release(sharpened);

            Profiler.EndEventGPU();
        }
    }
}
