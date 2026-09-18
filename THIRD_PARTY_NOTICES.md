# Third-party notices

3D Text+ itself is MIT-licensed (see [`LICENSE`](LICENSE)). It vendors the
following third-party libraries under `third_party/`, each under its own
permissive license (full text kept in the source file itself, or in a
`LICENSE.txt` alongside it):

| Library | Used for | License |
|---|---|---|
| [stb_truetype](https://github.com/nothings/stb) | Font parsing / glyph outlines | MIT / Unlicense (dual) |
| [stb_image](https://github.com/nothings/stb) | Image loading (background/env images) | MIT / Unlicense (dual) |
| [stb_image_write](https://github.com/nothings/stb) | PNG export (self-test screenshots) | MIT / Unlicense (dual) |
| [libtess2](https://github.com/memononen/libtess2) | Polygon tessellation (flat caps) | SGI Free Software License B |
| [Clipper2](https://github.com/AngusJohnson/Clipper2) | Robust polygon offsetting (bevel) | Boost Software License 1.0 |
| [nanosvg](https://github.com/memononen/nanosvg) | SVG parsing | zlib |
| [fast_obj](https://github.com/thisistherk/fast_obj) | OBJ mesh import | MIT |
| [cgltf](https://github.com/jkuhlmann/cgltf) | glTF mesh import | MIT |
| [glad](https://glad.dav1d.de/) (generated loader) + Khronos `khrplatform.h` | OpenGL function loading | CC0-1.0 / WTFPL + Apache-2.0 |

The default environment reflection image ([`res/env_default.jpg`](res/env_default.jpg))
is `boma_1k` from [Poly Haven](https://polyhaven.com/), CC0.
