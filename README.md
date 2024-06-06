# ShadeD (OpenGL)

This is just a reimplementation of ShadeD under native OpenGL for cross-platform support. It runs a supplied fragment shader rendering it to a quad that's the size of the screen, this can allow for easier leverage of per-pixel calculated visuals and rendering such as ray marching and path tracing passed under the GPU (see online tools like [Shadertoy](https://shadertoy.com) to see some more example shaders). This project was originally created under an older project I made, mV3D, which was created under WPF with a browser WebGL renderer. For convenience, I made the shader uniform variables the same as Shadertoy's definitions.

## Usage

As of now you just supply the fragment shader file (will plan to add more options)

```
./shaded <glsl-fragment-shader>
```
