using System;
using System.IO;
using FlaxEngine;

namespace FidelityFX
{
    /// <summary>
    /// FidelityFX Super Resolution plugin.
    /// </summary>
    public sealed class FSR : GamePlugin
    {
        /// <inheritdoc />
        public override PluginDescription Description => new PluginDescription
        {
            Name = "AMD FidelityFX Super Resolution 1.0",
            Category = "Rendering",
            Description = "AMD Fidelity FX Super Resolution 1.0 is a cutting edge super-optimize spatial upsampling technology that produces impressive image quality at fast framerates.",
            Author = "AMD",
            RepositoryUrl = "https://github.com/FlaxEngine/FidelityFX-FSR",
            Version = new Version(1, 0),
        };

        internal static Guid ShaderId = FlaxEngine.Json.JsonSerializer.ParseID("0000012c0b87e7b00000004600000052");

        private FSRPostFx _postFx;

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
            SceneRenderTask.GlobalCustomPostFx.Add(_postFx = new FSRPostFx());
        }

        /// <inheritdoc />
        public override void Deinitialize()
        {
            SceneRenderTask.GlobalCustomPostFx.Remove(_postFx);
            FlaxEngine.Object.Destroy(ref _postFx);

            base.Deinitialize();
        }

#if FLAX_EDITOR
        /// <inheritdoc />
        public override void OnCollectAssets(System.Collections.Generic.List<Guid> assets)
        {
            base.OnCollectAssets(assets);

            assets.Add(ShaderId);

            var path = Path.Combine(Globals.ProjectFolder, "Plugins/MyPlugin/Content/MyCustomDebugModel.flax");
            Content.GetAssetInfo(path, out var info);
            assets.Add(info.ID);
        }
#endif
    }
}