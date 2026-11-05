/* SPDX-License-Identifier: MIT */
#ifndef G3_NATIVE_MATH_H
#define G3_NATIVE_MATH_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Row vectors, row-major matrices, signed 20.12. No floating-point arithmetic.
 * Products accumulate modulo 2^64, then arithmetic shift by 12 and narrow to
 * 32 bits. In particular, this is NOT Nitro FX_Mul's per-product rounding.
 * All batch operations accept contiguous arrays; input/output may alias.
 * A batch is one state span: reuse its matrix across all vertices. Backends
 * may block that span without crossing a command/state boundary. */
typedef struct { int32_t m[4][4]; } G3NativeMatrix;
typedef struct { int32_t v[4]; } G3NativeVector;
typedef struct {
    int32_t position[4];
    int32_t color[4]; /* RGB and alpha, 0..31; retained through clipping */
    int32_t texcoord[2]; /* signed 12.4 */
    int32_t reserved[2];
} G3NativeVertex;
typedef struct {
    int32_t direction[4][3], half[4][3]; /* signed .9 */
    uint8_t color[4][3], diffuse[3], ambient[3], specular[3], emission[3];
    uint8_t enabled, use_shininess, shininess[128];
} G3NativeLighting;
void G3Native_Identity(G3NativeMatrix *out);
void G3Native_MultiplyScalar(G3NativeMatrix *out, const G3NativeMatrix *a,
                            const G3NativeMatrix *b, size_t count);
void G3Native_TransformScalar(G3NativeVector *out, const G3NativeVector *in,
                             size_t count, const G3NativeMatrix *matrix);
void G3Native_LightScalar(uint8_t (*out)[3], const G3NativeVector *normal,
                         size_t count, const G3NativeLighting *lights);
/* Convex DS triangles/quads clip to at most ten vertices. Returns -1 on bad
 * arguments/capacity. far_clip=0 rejects polygons intersecting the far plane. */
int G3Native_ClipScalar(G3NativeVertex *out, size_t capacity,
                      const G3NativeVertex *in, size_t count, int far_clip);
/* Immutable dispatch seam. Optimized implementations must match scalar bits;
 * no state management, allocations, callbacks or GPU submission live here. */
typedef struct {
    void (*multiply)(G3NativeMatrix *, const G3NativeMatrix *, const G3NativeMatrix *, size_t);
    void (*transform)(G3NativeVector *, const G3NativeVector *, size_t, const G3NativeMatrix *);
    void (*light)(uint8_t (*)[3], const G3NativeVector *, size_t, const G3NativeLighting *);
    int (*clip)(G3NativeVertex *, size_t, const G3NativeVertex *, size_t, int);
} G3NativeMath;
extern const G3NativeMath G3Native_Scalar;
#ifdef __cplusplus
}
#endif
#endif
