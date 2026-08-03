# Profile Image Pipeline

Status: **implemented for the Windows desktop preview; Onion target verification pending**.

## Boundary

The launcher imports a local BMP, JPEG, or PNG into managed profile-image storage. It does not keep a source path, embed image bytes in SQLite or configuration, upload an image, or expose a general photo library. Profile rows retain only a `local:<asset-id>` reference.

The desktop build uses SDL2_image 2.8.8 at commit `c1bf2245b0ba63a25afe2f8574d305feca25af77` with its zlib license. The build enables only its built-in stb backend plus BMP, JPEG, and PNG decoding and JPEG/PNG encoding. AVIF, GIF, JPEG-XL, TIFF, WebP, SVG, and other format stacks are disabled. SDL2_image reuses the existing SDL boundary; handwritten codec logic and desktop-only imaging frameworks were rejected. The dependency is maintained upstream and portable C, but Onion toolchain compatibility and release-binary impact remain unverified.

## Validation and normalization

Sprout ships 32 reviewed built-in portraits. Each built-in has a transparent
512Ã—512 master and a 128Ã—128 runtime thumbnail; the launcher uses the thumbnail
for profile grids and selectors rather than decoding the master in small
contexts. Built-in references are restricted to the maintained catalogue and
remain ordinary `builtin:<id>` values in profile storage.

Built-in artwork stores only the borderless subject on transparency. The
launcher derives a feathered outline from the subject alpha once, caches the
resulting texture, and chooses its color at render time. Focus uses the theme
focus color, normal profile cards use the profile accent, and catalogue cells
use the neutral theme color. Artwork therefore keeps its smooth illustrated
edge without fixing the interface to a green or any other baked-in border.

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

The source image is not preserved in v1. A portable profile export can include only the normalized managed portrait and thumbnail after explicit parent consent. These PNGs are stored unencrypted in the archive; source metadata and source paths remain excluded. See the [portable profile archive specification](profile-archive.md). Orphan recovery remains deferred.

## Setup presentation

First-run setup checks only the data directory's `imports/` folder for `profile-image.png`, `profile-image.jpg`, `profile-image.jpeg`, or `profile-image.bmp`, in that order. The portrait step can open the complete built-in catalogue for the parent or child and, when a staged image exists, explicitly import it for the parent. The staged source path is never persisted.

After setup, an authenticated parent opens **Profile Settings**, chooses any
active household profile, and assigns a built-in portrait. If a staged import
exists, the same screen exposes **Import Custom Image** for the selected
profile and routes through the existing crop/validation pipeline. Changing a
portrait requires fresh parent PIN verification; child mode cannot enter this
surface.

The crop screen decodes and orients the source once, then supports D-pad or arrow-key positioning and bounded zoom with the controller shoulder buttons or Q/E. Confirm activates the rendered crop; Back cancels without changing the profile. A missing or invalid staged image returns a visible error at the portrait step. Managed portraits are resolved from their validated `local:` reference and rendered in the profile selector. Built-in thumbnails are resolved only from the packaged catalogue; either path falls back to the profile initial if its file cannot be loaded.
