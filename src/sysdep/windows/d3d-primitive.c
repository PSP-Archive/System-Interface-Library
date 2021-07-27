/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/d3d-primitive.c: Primitive rendering functionality
 * for Direct3D.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/endian.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/d3d-internal.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Semantic names for custom shader vertex attributes.  Initialized on the
 * first call to d3d_sys_graphics_create_primitive(). */
static char attrib_name[D3D11_VS_INPUT_REGISTER_COUNT][16];

/* Constant index buffer for single quads. */
static ID3D11Buffer *single_quad_index_buffer;

/* Reusable vertex and index buffers for dynamic primitives. */
// FIXME: not yet used
//static ID3D11Buffer *immediate_vertex_buffer[128];
//static ID3D11Buffer *immediate_index_buffer[128];
//static int next_immediate_buffer;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

D3DSysPrimitive *d3d_sys_graphics_create_primitive(
    enum GraphicsPrimitiveType type, const void *data, const uint32_t *format,
    int size, int count, const void *index_data, int index_size,
    int index_count, int immediate)
{
    if (UNLIKELY(!*attrib_name[0])) {
        for (int i = 0; i < lenof(attrib_name); i++) {
            ASSERT(strformat_check(attrib_name[i], sizeof(attrib_name[i]),
                                   "ATTRIBUTE%d", i));
        }
    }

    void *local_vertex_data = NULL; // Freed (if not NULL) on return.
    void *local_index_data = NULL;  // Freed (if not NULL) on return.
    int need_quad_indices = 0;      // Set true if we need to generate indices.
    HRESULT result;

    /* Allocate memory for the primitive object. */
    int mem_flags = MEM_ALLOC_CLEAR;
    if (immediate) {
        mem_flags |= MEM_ALLOC_TEMP;
    }
    D3DSysPrimitive *primitive = mem_alloc(sizeof(*primitive), 0, mem_flags);
    if (UNLIKELY(!primitive)) {
        DLOG("No memory for D3DSysPrimitive structure");
        goto error_return;
    }
    primitive->generation       = d3d_device_generation;
    primitive->type             = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    primitive->render_count     = index_data ? index_count : count;
    primitive->vertex_size      = size;
    primitive->index_type       = DXGI_FORMAT_UNKNOWN;

    /* Convert the SIL primitive type to a Direct3D type.  The caller
     * guarantees that the incoming type is valid. */
    switch (type) {
      case GRAPHICS_PRIMITIVE_POINTS:
        primitive->type = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        break;
      case GRAPHICS_PRIMITIVE_LINES:
        primitive->type = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        break;
      case GRAPHICS_PRIMITIVE_LINE_STRIP:
        primitive->type = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        break;
      case GRAPHICS_PRIMITIVE_TRIANGLES:
        primitive->type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
      case GRAPHICS_PRIMITIVE_TRIANGLE_STRIP:
        primitive->type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        break;
      case GRAPHICS_PRIMITIVE_QUADS:
        primitive->type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        primitive->converted_quads = 1;
        if (primitive->render_count >= 4) {
            need_quad_indices = 1;
        } else {
            /* Continue constructing a primitive object for consistent
             * behavior, but make sure we don't try to render a single
             * triangle if we got passed 3 vertices. */
            primitive->render_count = 1;
        }
        break;
      case GRAPHICS_PRIMITIVE_QUAD_STRIP:
        primitive->type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        primitive->converted_quads = 1;
        if (primitive->render_count < 4) {
            primitive->render_count = 1;
        } else {
            /* In addition to the above check, make sure we don't draw
             * half a quad if the vertex count is odd. */
            primitive->render_count &= ~1;
        }
        break;
    }
    ASSERT(primitive->type != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED,
           goto error_return);

    /* Convert the input vertex format array to its Direct3D equivalent. */
    int need_vertex_copy = 0;
    D3D11_INPUT_ELEMENT_DESC input_elements[D3D11_VS_INPUT_REGISTER_COUNT];
    int num_inputs = 0;
    for (int i = 0; format[i] != 0; i++, num_inputs++) {
        if (i >= lenof(input_elements)) {
            DLOG("Too many input elements (max %d)", lenof(input_elements));
            goto error_free_primitive;
        }

        input_elements[i].SemanticName = NULL;
        input_elements[i].SemanticIndex = 0;
        input_elements[i].Format = DXGI_FORMAT_UNKNOWN;
        input_elements[i].InputSlot = 0;
        input_elements[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        input_elements[i].InstanceDataStepRate = 0;
        input_elements[i].AlignedByteOffset =
            GRAPHICS_VERTEX_FORMAT_OFFSET(format[i]);

        /* Direct3D seems to require 4-byte alignment even for narrower
         * data types. */
        if (input_elements[i].AlignedByteOffset % 4 != 0) {
            need_vertex_copy = 1;
        }

        int vertex_type = GRAPHICS_VERTEX_FORMAT_TYPE(format[i]);
        int type_only = vertex_type;
        if (GRAPHICS_VERTEX_TYPE_IS_ATTRIB(vertex_type)) {
            type_only &= 0xFF00;  // Mask off index for type checking.
        }
        switch (type_only) {
          case GRAPHICS_VERTEX_ATTRIB_1UB(0):
            input_elements[i].Format = DXGI_FORMAT_R8_UINT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_1S(0):
            input_elements[i].Format = DXGI_FORMAT_R16_SINT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_1I(0):
            input_elements[i].Format = DXGI_FORMAT_R32_SINT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_1NUB(0):
            input_elements[i].Format = DXGI_FORMAT_R8_UNORM;
            break;
          case GRAPHICS_VERTEX_ATTRIB_1NS(0):
            input_elements[i].Format = DXGI_FORMAT_R16_SNORM;
            break;
          case GRAPHICS_VERTEX_ATTRIB_1F(0):
            input_elements[i].Format = DXGI_FORMAT_R32_FLOAT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_2UB(0):
            input_elements[i].Format = DXGI_FORMAT_R8G8_UINT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_2S(0):
            input_elements[i].Format = DXGI_FORMAT_R16G16_SINT;
            break;
          case GRAPHICS_VERTEX_POSITION_2S:
            need_vertex_copy = 1;
            input_elements[i].Format = DXGI_FORMAT_R32G32_FLOAT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_2I(0):
            input_elements[i].Format = DXGI_FORMAT_R32G32_SINT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_2NUB(0):
            input_elements[i].Format = DXGI_FORMAT_R8G8_UNORM;
            break;
          case GRAPHICS_VERTEX_ATTRIB_2NS(0):
            input_elements[i].Format = DXGI_FORMAT_R16G16_SNORM;
            break;
          case GRAPHICS_VERTEX_ATTRIB_2F(0):
          case GRAPHICS_VERTEX_POSITION_2F:
          case GRAPHICS_VERTEX_TEXCOORD_2F:
            input_elements[i].Format = DXGI_FORMAT_R32G32_FLOAT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_3UB(0):
          case GRAPHICS_VERTEX_ATTRIB_3S(0):
            need_vertex_copy = 1;
            /* Fall through. */
          case GRAPHICS_VERTEX_ATTRIB_3I(0):
            input_elements[i].Format = DXGI_FORMAT_R32G32B32_SINT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_3NUB(0):
          case GRAPHICS_VERTEX_ATTRIB_3NS(0):
            need_vertex_copy = 1;
            /* Fall through. */
          case GRAPHICS_VERTEX_ATTRIB_3F(0):
          case GRAPHICS_VERTEX_POSITION_3F:
            input_elements[i].Format = DXGI_FORMAT_R32G32B32_FLOAT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_4UB(0):
            input_elements[i].Format = DXGI_FORMAT_R8G8B8A8_UINT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_4S(0):
            input_elements[i].Format = DXGI_FORMAT_R16G16B16A16_SINT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_4I(0):
            input_elements[i].Format = DXGI_FORMAT_R32G32B32A32_SINT;
            break;
          case GRAPHICS_VERTEX_ATTRIB_4NUB(0):
          case GRAPHICS_VERTEX_COLOR_4NUB:
            input_elements[i].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
          case GRAPHICS_VERTEX_ATTRIB_4NS(0):
            input_elements[i].Format = DXGI_FORMAT_R16G16B16A16_SNORM;
            break;
          case GRAPHICS_VERTEX_ATTRIB_4F(0):
          case GRAPHICS_VERTEX_POSITION_4F:
          case GRAPHICS_VERTEX_COLOR_4F:
            input_elements[i].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            break;
        }

        switch (vertex_type) {
          case GRAPHICS_VERTEX_POSITION_2S:
          case GRAPHICS_VERTEX_POSITION_2F:
            input_elements[i].SemanticName = "POSITION";
            primitive->has_position = 1;
            primitive->position_count = 2;
            break;
          case GRAPHICS_VERTEX_POSITION_3F:
            input_elements[i].SemanticName = "POSITION";
            primitive->has_position = 1;
            primitive->position_count = 3;
            break;
          case GRAPHICS_VERTEX_POSITION_4F:
            input_elements[i].SemanticName = "POSITION";
            primitive->has_position = 1;
            primitive->position_count = 4;
            break;
          case GRAPHICS_VERTEX_TEXCOORD_2F:
            input_elements[i].SemanticName = "TEXCOORD";
            primitive->has_texcoord = 1;
            break;
          case GRAPHICS_VERTEX_COLOR_4NUB:
          case GRAPHICS_VERTEX_COLOR_4F:
            input_elements[i].SemanticName = "COLOR";
            primitive->has_color = 1;
            break;
          default:
            if (GRAPHICS_VERTEX_TYPE_IS_ATTRIB(vertex_type)) {
                const int index = GRAPHICS_VERTEX_ATTRIB_INDEX(vertex_type);
                if (index < lenof(attrib_name)) {
                    input_elements[i].SemanticName =
                        attrib_name[index];
                }
            }
            break;
        }

        if (!input_elements[i].SemanticName
         || input_elements[i].Format == DXGI_FORMAT_UNKNOWN) {
            DLOG("Unknown vertex data format 0x%08X, aborting", format[i]);
                goto error_free_primitive;
        }
    }

    /* If any of the input types or alignments were invalid for Direct3D,
     * generate a temporary copy of the data with proper types/alignments. */
    if (need_vertex_copy) {
        DLOG("Warning: reformatting primitive data for Direct3D");
        int new_size = 0;
        for (int i = 0; i < num_inputs; i++) {
            input_elements[i].AlignedByteOffset = new_size;
            const int elem_size = d3d_format_bpp(input_elements[i].Format) / 8;
            ASSERT(elem_size > 0);
            new_size += align_up(elem_size, 4);
        }
        local_vertex_data = mem_alloc(count * new_size, 4, MEM_ALLOC_TEMP);
        if (UNLIKELY(!local_vertex_data)) {
            DLOG("No memory for copy of vertex data (%d * %d bytes)",
                 count, new_size);
            goto error_free_primitive;
        }
        const uint8_t *src = data;
        uint8_t *dest = local_vertex_data;
        for (int i = 0; i < count; i++, src += size, dest += new_size) {
            for (int j = 0; j < num_inputs; j++) {
                const void *input_src =
                    src + GRAPHICS_VERTEX_FORMAT_OFFSET(format[j]);
                void *input_dest = dest + input_elements[j].AlignedByteOffset;
                int type_only = GRAPHICS_VERTEX_FORMAT_TYPE(format[j]);
                if (GRAPHICS_VERTEX_TYPE_IS_ATTRIB(type_only)) {
                    type_only &= 0xFF00;
                }
                switch (type_only) {
                  case GRAPHICS_VERTEX_POSITION_2S:
                    for (int component = 0; component < 2; component++) {
                        ((float *)input_dest)[component] =
                            (float) ((const int16_t *)input_src)[component];
                    }
                    break;
                  case GRAPHICS_VERTEX_ATTRIB_3UB(0):
                    for (int component = 0; component < 3; component++) {
                        ((int32_t *)input_dest)[component] =
                            ((const int16_t *)input_src)[component];
                    }
                    break;
                  case GRAPHICS_VERTEX_ATTRIB_3S(0):
                    for (int component = 0; component < 3; component++) {
                        ((int32_t *)input_dest)[component] =
                            ((const int16_t *)input_src)[component];
                    }
                    break;
                  case GRAPHICS_VERTEX_ATTRIB_3NUB(0):
                    for (int component = 0; component < 3; component++) {
                        ((float *)input_dest)[component] =
                            ((const uint8_t *)input_src)[component] / 255.0f;
                    }
                    break;
                  case GRAPHICS_VERTEX_ATTRIB_3NS(0):
                    for (int component = 0; component < 3; component++) {
                        ((float *)input_dest)[component] =
                            ((int)((const int16_t *)input_src)[component]
                             + 32768) / 32767.5f + 1.0f;
                    }
                    break;
                  default:
                    memcpy(input_dest, input_src,
                           d3d_format_bpp(input_elements[j].Format) / 8);
                    break;
                }
            }
        }
        data = local_vertex_data;
        primitive->vertex_size = size = new_size;
    }

    /* Create a Direct3D InputLayout for the (possibly modified) vertex
     * format. */
    primitive->input_layout = d3d_inputlayout_get(input_elements, num_inputs);
    if (UNLIKELY(!primitive->input_layout)) {
        DLOG("Failed to create input layout for primitive");
        goto error_free_primitive;
    }

    /* If we were given 16-bit index data, check that none of the index
     * values is 65535 (the maximum 16-bit value); if any are, convert the
     * index data to 32-bit.  This is required because Direct3D has no way
     * to disable the primitive restart index.  Grr. */
    if (index_size == 2) {
        ASSERT(index_data);
        ASSERT(!local_index_data);
        int need_convert = 0;
        for (int i = 0; i < index_count; i++) {
            if (((const uint16_t *)index_data)[i] == 65535) {
                need_convert = 1;
                break;
            }
        }
        if (need_convert) {
            local_index_data = mem_alloc(4 * index_count, 0, MEM_ALLOC_TEMP);
            if (UNLIKELY(!local_index_data)) {
                DLOG("Failed to allocate temporary index buffer (%d bytes)",
                     4 * index_count);
                goto error_free_primitive;
            }
            for (int i = 0; i < index_count; i++) {
                ((uint32_t *)local_index_data)[i] =
                    ((const uint16_t *)index_data)[i];
            }
            index_data = local_index_data;
            index_size = 4;
        }
    }

    /* If we were given a byte-type index buffer, convert bytes to shorts
     * because Direct3D 11 doesn't support single-byte indices. */
    if (index_size == 1) {
        ASSERT(index_data);
        ASSERT(!local_index_data);
        local_index_data = mem_alloc(2 * index_count, 0, MEM_ALLOC_TEMP);
        if (UNLIKELY(!local_index_data)) {
            DLOG("Failed to allocate temporary index buffer (%d bytes)",
                 2 * index_count);
            goto error_free_primitive;
        }
        for (int i = 0; i < index_count; i++) {
            ((uint16_t *)local_index_data)[i] =
                ((const uint8_t *)index_data)[i];
        }
        index_data = local_index_data;
        index_size = 2;
    }

    /* If rendering quads, convert each quad to 2 triangles. */
    if (need_quad_indices) {
        const int num_quads = primitive->render_count / 4;
        ASSERT(num_quads > 0);

        if (num_quads == 1 && !index_data) {
            /* If this is a single, non-indexed quad, use a common index
             * buffer to conserve resources. */
            if (!single_quad_index_buffer) {
                static const uint16_t single_quad_indices[6] = {0,1,3,3,1,2};
                D3D11_BUFFER_DESC buffer_desc = {
                    .ByteWidth = sizeof(single_quad_indices),
                    .Usage = D3D11_USAGE_IMMUTABLE,
                    .BindFlags = D3D11_BIND_INDEX_BUFFER,
                    .CPUAccessFlags = 0,
                    .MiscFlags = 0,
                    .StructureByteStride = 0,
                };
                D3D11_SUBRESOURCE_DATA buffer_data = {
                    .pSysMem = single_quad_indices,
                    .SysMemPitch = sizeof(single_quad_indices),
                    .SysMemSlicePitch = sizeof(single_quad_indices),
                };
                result = ID3D11Device_CreateBuffer(
                    d3d_device, &buffer_desc, &buffer_data,
                    &single_quad_index_buffer);
                if (UNLIKELY(result != S_OK)) {
                    DLOG("Failed to generate single quad index buffer");
                    goto error_free_primitive;
                }
            }
            primitive->index_buffer = single_quad_index_buffer;
            primitive->index_type = DXGI_FORMAT_R16_UINT;
            primitive->is_single_quad = 1;
            primitive->render_count = 6;

        } else {
            /* Multiple quads or primitive is indexed, so generate new data. */
            void *old_local_index_data = local_index_data;
            int quad_index_size;
            if (index_data) {
                ASSERT(index_size != 1);
                quad_index_size = index_size;
            } else if (4*num_quads <= 65532) { // Not 65536, see above for why.
                quad_index_size = 2;
            } else {
                quad_index_size = 4;
            }
            local_index_data = mem_alloc(quad_index_size * (6*num_quads), 0,
                                         MEM_ALLOC_TEMP);
            if (UNLIKELY(!local_index_data)) {
                DLOG("No memory for quad index list (%u*%u bytes)",
                     quad_index_size, 6*num_quads);
                mem_free(old_local_index_data);
                goto error_free_primitive;
            }
            for (int i = 0; i < num_quads; i++) {
                uint32_t A, B, C, D;  // Vertex indices of this quad.
                if (index_data) {
                    if (index_size == 1) {
                        A = ((const uint8_t *)index_data)[4*i+0];
                        B = ((const uint8_t *)index_data)[4*i+1];
                        C = ((const uint8_t *)index_data)[4*i+2];
                        D = ((const uint8_t *)index_data)[4*i+3];
                    } else if (index_size == 2) {
                        A = ((const uint16_t *)index_data)[4*i+0];
                        B = ((const uint16_t *)index_data)[4*i+1];
                        C = ((const uint16_t *)index_data)[4*i+2];
                        D = ((const uint16_t *)index_data)[4*i+3];
                    } else {  // index_size == 4
                        A = ((const uint32_t *)index_data)[4*i+0];
                        B = ((const uint32_t *)index_data)[4*i+1];
                        C = ((const uint32_t *)index_data)[4*i+2];
                        D = ((const uint32_t *)index_data)[4*i+3];
                    }
                } else {  // !index_data
                    A = 4*i+0;
                    B = 4*i+1;
                    C = 4*i+2;
                    D = 4*i+3;
                }
                if (quad_index_size == 2) {
                    ((uint16_t *)local_index_data)[6*i+0] = A;
                    ((uint16_t *)local_index_data)[6*i+1] = B;
                    ((uint16_t *)local_index_data)[6*i+2] = D;
                    ((uint16_t *)local_index_data)[6*i+3] = D;
                    ((uint16_t *)local_index_data)[6*i+4] = B;
                    ((uint16_t *)local_index_data)[6*i+5] = C;
                } else {  // quad_index_size == 4
                    ((uint32_t *)local_index_data)[6*i+0] = A;
                    ((uint32_t *)local_index_data)[6*i+1] = B;
                    ((uint32_t *)local_index_data)[6*i+2] = D;
                    ((uint32_t *)local_index_data)[6*i+3] = D;
                    ((uint32_t *)local_index_data)[6*i+4] = B;
                    ((uint32_t *)local_index_data)[6*i+5] = C;
                }
            }
            index_data = local_index_data;
            index_size = quad_index_size;
            primitive->render_count = index_count = 6*num_quads;
            mem_free(old_local_index_data);
        }  // if (single quad optimization)
    }  // if (need_quad_indices)

    /* Create vertex and (if needed) index buffers for the primitive. */
    // FIXME: immediate buffers
    D3D11_BUFFER_DESC buffer_desc = {
        .ByteWidth = size * count,
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
        .StructureByteStride = size,
    };
    D3D11_SUBRESOURCE_DATA buffer_data = {
        .pSysMem = data,
        .SysMemPitch = buffer_desc.ByteWidth,
        .SysMemSlicePitch = buffer_desc.ByteWidth,
    };
    result = ID3D11Device_CreateBuffer(d3d_device, &buffer_desc, &buffer_data,
                                       &primitive->vertex_buffer);
    if (UNLIKELY(result != S_OK)) {
        DLOG("Failed to create vertex buffer");
        goto error_free_primitive;
    }
    if (index_data) {
        if (index_size == 2) {
            primitive->index_type = DXGI_FORMAT_R16_UINT;
        } else {
            ASSERT(index_size == 4);
            primitive->index_type = DXGI_FORMAT_R32_UINT;
        }
        buffer_desc.ByteWidth = index_size * index_count;
        buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        buffer_desc.StructureByteStride = index_size;
        buffer_data.pSysMem = index_data;
        buffer_data.SysMemPitch = buffer_desc.ByteWidth;
        buffer_data.SysMemSlicePitch = buffer_desc.ByteWidth;
        result = ID3D11Device_CreateBuffer(
            d3d_device, &buffer_desc, &buffer_data,
            &primitive->index_buffer);
        if (UNLIKELY(result != S_OK)) {
            DLOG("Failed to create index buffer");
            goto error_free_primitive;
        }
    }

    /* Return the new primitive. */
    mem_free(local_index_data);
    mem_free(local_vertex_data);
    return primitive;

  error_free_primitive:
    if (primitive->vertex_buffer && !primitive->is_immediate_vbo) {
        ID3D11Buffer_Release(primitive->vertex_buffer);
    }
    if (primitive->index_buffer && !primitive->is_single_quad) {
        ID3D11Buffer_Release(primitive->index_buffer);
    }
    if (primitive->input_layout) {
        ID3D11InputLayout_Release(primitive->input_layout);
    }
    mem_free(primitive);
  error_return:
    mem_free(local_index_data);
    mem_free(local_vertex_data);
    return NULL;
}

/*-----------------------------------------------------------------------*/

void d3d_sys_graphics_draw_primitive(D3DSysPrimitive *primitive,
                                     int start, int count)
{
    if (UNLIKELY(primitive->generation != d3d_device_generation)) {
        DLOG("Attempt to draw invalidated primitive %p", primitive);
        return;
    }

    d3d_state_apply();
    if (!d3d_shader_objects_enabled) {
        d3d_state_set_shader(primitive);
    }

    int draw_unit = 1;
    if (primitive->converted_quads) {
        if (primitive->type == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
            if (UNLIKELY(start % 4 != 0)) {
                DLOG("WARNING: unaligned partial draw of converted QUADS"
                     " primitive (start=%d count=%d)", start, count);
            }
            start = ((start/4) * 6) + (start % 4);
            if (count > 0) {
                count = (count/4) * 6;
            }
            draw_unit = 6;
        } else {  // Must be converted from QUAD_STRIP.
            if (UNLIKELY(start % 2 != 0)) {
                DLOG("WARNING: unaligned partial draw of converted QUAD_STRIP"
                     " primitive (start=%d count=%d)", start, count);
            }
            draw_unit = 2;
        }
    }

    if (start >= primitive->render_count) {
        return;
    }
    if (count < 0 || count > primitive->render_count - start) {
        count = primitive->render_count - start;
    }
    if (draw_unit > 1) {
        count -= count % draw_unit;
    }
    if (count <= 0) {
        return;
    }

    ID3D11DeviceContext_IASetPrimitiveTopology(d3d_context, primitive->type);
    ID3D11DeviceContext_IASetInputLayout(d3d_context, primitive->input_layout);
    ID3D11DeviceContext_IASetVertexBuffers(
        d3d_context, 0, 1, &primitive->vertex_buffer,
        ((UINT[]){primitive->vertex_size}), ((UINT[]){0}));
    if (primitive->index_buffer) {
        ID3D11DeviceContext_IASetIndexBuffer(
            d3d_context, primitive->index_buffer, primitive->index_type, 0);
        ID3D11DeviceContext_DrawIndexed(d3d_context, count, start, 0);
    } else {
        ID3D11DeviceContext_Draw(d3d_context, count, start);
    }
}

/*-----------------------------------------------------------------------*/

void d3d_sys_graphics_destroy_primitive(D3DSysPrimitive *primitive)
{
    if (primitive->generation == d3d_device_generation) {
        if (!primitive->is_immediate_vbo) {
            ID3D11Buffer_Release(primitive->vertex_buffer);
            if (primitive->index_buffer && !primitive->is_single_quad) {
                ID3D11Buffer_Release(primitive->index_buffer);
            }
        }
        if (primitive->input_layout) {
            ID3D11InputLayout_Release(primitive->input_layout);
        }
    }
    mem_free(primitive);
}

/*************************************************************************/
/******** Internal interface routines (private to Direct3D code) *********/
/*************************************************************************/

void d3d_primitive_cleanup(void)
{
    if (single_quad_index_buffer) {
        ID3D11Buffer_Release(single_quad_index_buffer);
        single_quad_index_buffer = NULL;
    }
}

/*************************************************************************/
/*************************************************************************/
