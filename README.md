# AMD FidelityFX Super Resolution for Flax Engine

![AMD FidelityFX Super Resolution for Flax Engine](amd-fsr-flax.png)

[AMD Fidelity FX Super Resolution](https://gpuopen.com/fidelityfx-superresolution/) is a cutting edge super-optimize spatial upsampling technology that produces impressive image quality at fast framerates. This repository contains a plugin project for [Flax Engine](https://flaxengine.com/) games with FSR.

Minimum supported Flax version: `1.2`.

## Installation

1. Clone repo into `<game-project>\Plugins\FidelityFX-FSR`

2. Add reference to FSR project in your game by modyfying your game `<game-project>.flaxproj` as follows:


```
...
"References": [
	{
		"Name": "$(EnginePath)/Flax.flaxproj"
	},
	{
		"Name": "$(ProjectPath)/Plugins/FidelityFX-FSR/FidelityFX-FSR.flaxproj"
	}
```

3. Test it out!


Finally open Flax Editor - FSR will be visible in Plugins window (under Rendering category). It implements `CustomUpscale` postFx to increase visual quality when using low-res rendering. To test it simply start the game and adjust the **Rendering Percentage** property in *Graphics Quality Window*. Use scale factors provided by AMD to achieve the best quality-performance ratio. 

## License

Both this plugin and FSR are released under **MIT License**.

## Trademarks

© 2021 Advanced Micro Devices, Inc. All rights reserved. AMD, the AMD Arrow logo, Radeon, RDNA, Ryzen, and combinations thereof are trademarks of Advanced Micro Devices, Inc.
