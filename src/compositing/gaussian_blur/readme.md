# Gaussian Blur Example

This example shows off the following techniques:

- render to texture
- render a texture to another texture (per-pixel filtering)
  - renders a triangle internally
- push constants
- shader specialization constants (constant_id)
  - creates a unique version of a shader for a specific graphics pipeline
  - improves code reuse
- post processing of the texture (grayscale on part of the image)
- how to dynamically switch between textures to render in the final step
