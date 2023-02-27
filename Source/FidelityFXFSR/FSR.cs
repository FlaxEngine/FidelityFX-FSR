using System;
using FlaxEngine;

namespace FidelityFX
{
    /// <summary>
    /// FidelityFX Super Resolution plugin.
    /// </summary>
    public sealed class FSR : GamePlugin
    {
        internal static Guid ShaderId = FlaxEngine.Json.JsonSerializer.ParseID("0a5d1c1c48cdb7e3167993b32032c4fc");

        private FSRPostFx _postFx;

        /// <summary>
        /// Gets the FSR postfx.
        /// </summary>
        public FSRPostFx PostFx => _postFx;

        /// <inheritdoc />
        public FSR()
        {
            _description = new PluginDescription
            {
                Name = "AMD FidelityFX Super Resolution 1.0",
                Category = "Rendering",
                Description = "AMD Fidelity FX Super Resolution 1.0 is a cutting edge super-optimize spatial upsampling technology that produces impressive image quality at fast framerates.",
                Author = "AMD",
                RepositoryUrl = "https://github.com/FlaxEngine/FidelityFX-FSR",
                Version = new Version(1, 0, 1),
            };
        }

        /// <inheritdoc />
        public override void Initialize()
        {
            base.Initialize();

            if (!GPUDevice.Instance.Limits.HasCompute)
            {
                Debug.LogWarning("FSR is not supported on this platform.");
                return;
            }

            // TODO: apply global mip bias to texture groups samplers to increase texturing quality
            SceneRenderTask.AddGlobalCustomPostFx(_postFx = new FSRPostFx());
        }

        /// <inheritdoc />
        public override void Deinitialize()
        {
            SceneRenderTask.RemoveGlobalCustomPostFx(_postFx);
            FlaxEngine.Object.Destroy(ref _postFx);

            base.Deinitialize();
        }

#if FLAX_EDITOR
        /// <inheritdoc />
        public override Guid[] GetReferences()
        {
            return new[] { ShaderId };
        }
#endif
    }
}
