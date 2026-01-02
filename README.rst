convert-png-to-idtech3-tga
=========================

Small C99 tool that converts PNG images to idTech 3 compatible RLE TGAs
(image type 10, bottom-left origin).

Requirements
------------

- C compiler with C99 support
- Meson + Ninja
- libpng development package

Build
-----

.. code-block:: sh

   ./build.sh

Run
---

.. code-block:: sh

   ./build/convert-png-to-idtech3-tga input.png output.tga

Notes
-----

- Output is 32-bit TGA (BGRA) with alpha preserved; fully opaque if source has no alpha.
- Image origin is bottom-left to match idTech 3 expectations.
