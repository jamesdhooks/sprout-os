# Profile Image Pipeline

Status: **implemented import core; crop presentation in progress**.

## Boundary

The launcher imports a local BMP, JPEG, or PNG into managed profile-image storage. It does not keep a source path, embed image bytes in SQLite or configuration, upload an image, or expose a general photo library. Profile rows retain only a `local:<asset-id>` reference.

The desktop build uses SDL2_image 2.8.8 at commit `c1bf2245b0ba63a25afe2f8574d305feca25af77` with its zlib license. The build enables only its built-in stb backend plus BMP, JPEG, and PNG decoding and JPEG/PNG encoding. AVIF, GIF, JPEG-XL, TIFF, WebP, SVG, and other format stacks are disabled. SDL2_image reuses the existing SDL boundary; handwritten codec logic and desktop-only imaging frameworks were rejected. The dependency is maintained upstream and portable C, but Onion toolchain compatibility and release-binary impact remain unverified.

## Validation and normalization

- Source files must exist and be between 1 byte and 16 MiB.
- Decode dimensions are limited to 8,192 pixels per side and roughly 32 million pixels total.
- Malformed and unsupported input fails before the profile reference changes.
- JPEG EXIF orientation values 1 through 8 are applied before cropping.
- Crop center values are normalized to 0 through 1; zoom is limited to 1 through 4.
- The selected square is linearly resampled into a 256×256 portrait and a 96×96 thumbnail.
- Re-encoding to PNG strips source EXIF and unrelated metadata.

## Activation and cleanup

Each import writes a new profile-revision generation beneath managed image storage. Pending PNG files are synchronized and activated by same-directory rename before SQLite is updated. If activation or the profile update fails, the new generation is removed and the previous profile reference remains unchanged.

After a successful profile update, the prior managed generation is removed only when its reference has the expected safe identifier and its directory contains regular `portrait.png` and `thumbnail.png` files only. Unknown files, links, and malformed references stop cleanup rather than broadening it. Managed files request owner read/write permissions; physical SD-card access remains outside the parental-control security boundary.

The source image is not preserved in v1. Backup/export and orphan recovery require their own concrete consumer before changing retention behavior.
