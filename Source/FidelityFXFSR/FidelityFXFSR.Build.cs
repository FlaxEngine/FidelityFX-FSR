using Flax.Build;
using Flax.Build.NativeCpp;

public class FidelityFXFSR : GameModule
{
    /// <inheritdoc />
    public override void Setup(BuildOptions options)
    {
        base.Setup(options);

        BuildNativeCode = false;
    }
}
