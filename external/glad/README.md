Generated with glad 2 (https://github.com/Dav1dde/glad), version 2.0.8 (PyPI `glad2`),
on 31/10/2026:

```bash
python3 -m venv /tmp/glad && /tmp/glad/bin/pip install glad2==2.0.8
/tmp/glad/bin/glad --api gl:core=3.3 --extensions GL_KHR_debug \
    --out-path external/glad --reproducible c
```

- `include/glad/gl.h`, `src/gl.c`: OpenGL 3.3 core profile and `GL_KHR_debug`, without the
  built-in loader (`es-core/src/renderer/Renderer_init.cpp` passes `SDL_GL_GetProcAddress` to
  `gladLoadGL`). The core header does not declare the removed fixed-function entry points:
  calling one is a compile error.
- `include/KHR/khrplatform.h`: Khronos platform header.

Licences (from the glad package, copied in `glad_license.txt`): the generated code is
`(WTFPL OR CC0-1.0) AND Apache-2.0` (SPDX line at the top of `gl.h`); the glad generator is
MIT; the Khronos specifications it is generated from are Apache-2.0 and `khrplatform.h` is
under the Khronos MIT-style licence written in the file.

To regenerate (another extension, another version), run the same command and update this
file.
