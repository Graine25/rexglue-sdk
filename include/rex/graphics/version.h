/**
 * @file        graphics/version.h
 * @brief       Build metadata for the Xenos trace-writer format header.
 *
 * @remarks     Xenia's GPU trace writer stamps the build commit SHA into each
 *              trace file (`version.h` is normally build-generated). ReXGlue has
 *              no generated version header, so this supplies a placeholder SHA.
 *              TODO: wire to a real build-version source if trace provenance
 *              matters. The value must be >= 40 chars (memcpy'd into a 40-byte
 *              field).
 */

#ifndef REX_GRAPHICS_VERSION_H_
#define REX_GRAPHICS_VERSION_H_

#define XE_BUILD_COMMIT "0000000000000000000000000000000000000000"

#endif  // REX_GRAPHICS_VERSION_H_
