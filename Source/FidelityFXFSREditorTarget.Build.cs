using Flax.Build;

public class FidelityFXFSREditorTarget : GameProjectEditorTarget
{
    /// <inheritdoc />
    public override void Init()
    {
        base.Init();

        Modules.Add("FidelityFXFSR");
    }
}
