convert-png-to-idtech3-tga
=========================

Small tool that converts PNG images to idTech 3 compatible RLE TGAs
(image type 10, bottom-left origin).

Requirements
------------

- Go 1.20+

Build
-----

.. code-block:: sh

    go run ./build-tool/main.go [--all]

Run
---

.. code-block:: sh

   ./convert-png-to-idtech3-tga input.png [output.tga]

Notes
-----

- Output is 32-bit TGA (BGRA) with alpha preserved; fully opaque if source has no alpha.
- Image origin is bottom-left to match idTech 3 expectations.
