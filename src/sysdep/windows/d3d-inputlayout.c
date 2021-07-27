/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/d3d-inputlayout.c: InputLayout instance management
 * for Direct3D.
 */

/*
 * Direct3D requires an InputLayout object to store the association
 * between vertex attributes and buffer offsets, unlike most other APIs
 * which simply set the offsets at render time.  To make things worse,
 * these InputLayouts take a significant amount of time to create, to the
 * extent that immediate-rendering-heavy scenes can experience a
 * significant drop in frame rate.  To try and avoid this overhead, we
 * cache previously-generated InputLayout objects here, indexed by the SIL
 * vertex format description, and return a cached InputLayout object for
 * any previously-seen vertex format rather than creating a new one.
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

/**
 * HASH_LEN:  Size of the hash table to use for caching InputLayouts.
 */
#define HASH_LEN  8191

/**
 * VERIFY_INPUTLAYOUT_BYTECODE:  Define this to verify the bytecode
 * generated for InputLayout creation against an equivalent shader
 * compiled with D3DCompile().  This significantly slows down primitive
 * creation and requires d3dcompiler_47.dll to be available.
 */
#define VERIFY_INPUTLAYOUT_BYTECODE

/*-----------------------------------------------------------------------*/

/* Cache of generated InputLayouts and corresponding vertex formats. */
typedef struct ILCacheEntry ILCacheEntry;
struct ILCacheEntry {
    ILCacheEntry *next;
    ID3D11InputLayout *layout;
    int num_inputs;
    struct {
        char SemanticName[16];
        DXGI_FORMAT Format;
        unsigned int AlignedByteOffset;
    } input_elements[D3D11_VS_INPUT_REGISTER_COUNT];
};
static ILCacheEntry il_cache[HASH_LEN];

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * hash_format:  Return a hash value (table index) for the given vertex format.
 */
static PURE_FUNCTION int hash_format(
    const D3D11_INPUT_ELEMENT_DESC *input_elements, int num_inputs);

/**
 * create_input_layout:  Create and return an ID3D11InputLayout object for
 * the given set of input elements.
 */
static ID3D11InputLayout *create_input_layout(
    const D3D11_INPUT_ELEMENT_DESC *input_elements, int num_inputs);

/**
 * hlsl_checksum:  Compute the HLSL bytecode checksum for the given buffer.
 *
 * [Parameters]
 *     data: Data to checksum.
 *     size: Size of data, in bytes.
 *     checksum_ret: Pointer to 16-byte buffer to receive the checksum.
 */
static void hlsl_checksum(const void *data, int size, void *checksum_ret);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

ID3D11InputLayout *d3d_inputlayout_get(
    const D3D11_INPUT_ELEMENT_DESC *input_elements, int num_inputs)
{
    PRECOND(input_elements != NULL, return NULL);
    PRECOND(num_inputs > 0, return NULL);
    PRECOND(num_inputs <= D3D11_VS_INPUT_REGISTER_COUNT, return NULL);

    const int index = hash_format(input_elements, num_inputs);

    ILCacheEntry *entry = &il_cache[index];
    if (entry->layout) {
        for (; entry; entry = entry->next) {
            if (entry->num_inputs != num_inputs) {
                continue;
            }
            int match = 1;
            for (int i = 0; i < num_inputs; i++) {
                if (strcmp(input_elements[i].SemanticName,
                           entry->input_elements[i].SemanticName) != 0
                 || input_elements[i].Format != entry->input_elements[i].Format
                 || input_elements[i].AlignedByteOffset
                        != entry->input_elements[i].AlignedByteOffset) {
                    match = 0;
                    break;
                }
                if (match) {
                    ID3D11InputLayout_AddRef(entry->layout);
                    return entry->layout;
                }
            }
        }
    }

    ID3D11InputLayout *il = create_input_layout(input_elements, num_inputs);
    if (UNLIKELY(!il)) {
        return NULL;
    }

    entry = &il_cache[index];
    if (entry->layout) {
        while (entry->next) {
            entry = entry->next;
        }
        entry->next = mem_alloc(sizeof(*entry), 0, 0);
        entry = entry->next;
        if (UNLIKELY(!entry)) {
            DLOG("Failed to allocate memory for InputLayout cache entry");
        }
    }
    if (entry) {
        ID3D11InputLayout_AddRef(il);
        entry->layout = il;
        entry->num_inputs = num_inputs;
        for (int i = 0; i < num_inputs; i++) {
            ASSERT(strlen(input_elements[i].SemanticName)
                       < sizeof(entry->input_elements[i].SemanticName),
                   return NULL);
            strcpy(entry->input_elements[i].SemanticName,
                   input_elements[i].SemanticName);
            entry->input_elements[i].Format = input_elements[i].Format;
            entry->input_elements[i].AlignedByteOffset =
                input_elements[i].AlignedByteOffset;
        }
    }

    return il;
}

/*-----------------------------------------------------------------------*/

void d3d_inputlayout_free_all(void)
{
    for (int i = 0; i < lenof(il_cache); i++) {
        ILCacheEntry *entry = &il_cache[i];
        if (entry->layout) {
            ID3D11InputLayout_Release(entry->layout);
            entry = entry->next;
            while (entry) {
                ILCacheEntry *next = entry->next;
                ID3D11InputLayout_Release(entry->layout);
                mem_free(entry);
                entry = next;
            }
        }
    }

    mem_clear(il_cache, sizeof(il_cache));
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static PURE_FUNCTION int hash_format(
    const D3D11_INPUT_ELEMENT_DESC *input_elements, int num_inputs)
{
    uint32_t hash = 0;
    for (int i = 0; i < num_inputs; i++) {
        ASSERT(input_elements[i].SemanticIndex == 0);
        ASSERT(input_elements[i].InputSlot == 0);
        ASSERT(input_elements[i].InputSlotClass == D3D11_INPUT_PER_VERTEX_DATA);
        ASSERT(input_elements[i].InstanceDataStepRate == 0);
        uint32_t value = 0;
        for (const char *s = input_elements[i].SemanticName; *s; s++) {
            value = value<<1 ^ *s;
        }
        value = (value<<8 | value>>24) ^ input_elements[i].Format;
        value = (value<<8 | value>>24) ^ input_elements[i].AlignedByteOffset;
        hash = (hash>>5 | hash<<27) ^ value;
    }
    return hash % HASH_LEN;
}

/*-----------------------------------------------------------------------*/

static ID3D11InputLayout *create_input_layout(
    const D3D11_INPUT_ELEMENT_DESC *input_elements, int num_inputs)
{
    ASSERT(num_inputs <= D3D11_VS_INPUT_REGISTER_COUNT, return NULL);

    /* Direct3D bizarrely requires a copy of the bytecode of the vertex
     * shader in order to generate an InputLayout object.  We don't have
     * that, so we generate a dummy shader here and use it instead. */

    STATIC_ASSERT(
        D3D11_VS_INPUT_REGISTER_COUNT == 32,
        "bytecode_template assumes D3D11_VS_INPUT_REGISTER_COUNT == 32");
    #define ISGN_DECL(i)  \
        8+(i&15)*16, 3+(i>>4), \
                  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, \
          i,  0,  0,  0,  0,  0,  0,  0
    #define ISGN_NAMEDECL \
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    static const uint8_t bytecode_template[0x660] = {
        'D','X','B','C',  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  1,  0,  0,  0, 96,  6,  0,  0,  5,  0,  0,  0,
         52,  0,  0,  0, 92,  0,  0,  0,108,  5,  0,  0,160,  5,  0,  0,
        228,  5,  0,  0,'R','D','E','F', 32,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0, 28,  0,  0,  0,  0,  4,254,255,
          0, 65,  0,  0, 28,  0,  0,  0,'S','I','L',  0,'I','S','G','N',
          8,  5,  0,  0,  0,  0,  0,  0,  8,  0,  0,  0,
        ISGN_DECL( 0), ISGN_DECL( 1), ISGN_DECL( 2), ISGN_DECL( 3),
        ISGN_DECL( 4), ISGN_DECL( 5), ISGN_DECL( 6), ISGN_DECL( 7),
        ISGN_DECL( 8), ISGN_DECL( 9), ISGN_DECL(10), ISGN_DECL(11),
        ISGN_DECL(12), ISGN_DECL(13), ISGN_DECL(14), ISGN_DECL(15),
        ISGN_DECL(16), ISGN_DECL(17), ISGN_DECL(18), ISGN_DECL(19),
        ISGN_DECL(20), ISGN_DECL(21), ISGN_DECL(22), ISGN_DECL(23),
        ISGN_DECL(24), ISGN_DECL(25), ISGN_DECL(26), ISGN_DECL(27),
        ISGN_DECL(28), ISGN_DECL(29), ISGN_DECL(30), ISGN_DECL(31),
        ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL,
        ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL,
        ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL,
        ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL,
        ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL,
        ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL,
        ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL,
        ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL, ISGN_NAMEDECL,
                                                        'O','S','G','N',
         44,  0,  0,  0,  1,  0,  0,  0,  8,  0,  0,  0, 32,  0,  0,  0,
          0,  0,  0,  0,  1,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  0,
         15,  0,  0,  0,'S','V','_','P','o','s','i','t','i','o','n',  0,
        'S','H','D','R', 60,  0,  0,  0, 64,  0,  1,  0, 15,  0,  0,  0,
        103,  0,  0,  4,242, 32, 16,  0,  0,  0,  0,  0,  1,  0,  0,  0,
         54,  0,  0,  8,242, 32, 16,  0,  0,  0,  0,  0,  2, 64,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         62,  0,  0,  1,'S','T','A','T',116,  0,  0,  0,  2,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
          0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    };
    #undef ISGN_DECL
    #undef ISGN_NAMEDECL

    uint32_t bytecode[sizeof(bytecode_template) / 4];
    mem_clear(bytecode, sizeof(bytecode));
    memcpy(bytecode, bytecode_template, sizeof(bytecode_template));
    bytecode[0x64/4] = u32_to_le(num_inputs);
    for (int i = 0; i < num_inputs; i++) {
        uint32_t type, mask;
        switch (input_elements[i].Format) {
          case DXGI_FORMAT_R8_UINT:
            type = -1;  // FIXME type unknown
            mask = 1;
            break;
          case DXGI_FORMAT_R16_SINT:
          case DXGI_FORMAT_R32_SINT:
            type = -1;  // FIXME type unknown
            mask = 1;
            break;
          case DXGI_FORMAT_R8_UNORM:
          case DXGI_FORMAT_R16_SNORM:
          case DXGI_FORMAT_R32_FLOAT:
            type = 3;
            mask = 1;
            break;
          case DXGI_FORMAT_R8G8_UINT:
            type = -1;  // FIXME type unknown
            mask = 3;
            break;
          case DXGI_FORMAT_R16G16_SINT:
          case DXGI_FORMAT_R32G32_SINT:
            type = -1;  // FIXME type unknown
            mask = 3;
            break;
          case DXGI_FORMAT_R8G8_UNORM:
          case DXGI_FORMAT_R16G16_SNORM:
          case DXGI_FORMAT_R32G32_FLOAT:
            type = 3;
            mask = 3;
            break;
          case DXGI_FORMAT_R32G32B32_SINT:
            type = -1;  // FIXME type unknown
            mask = 7;
            break;
          case DXGI_FORMAT_R32G32B32_FLOAT:
            type = 3;
            mask = 7;
            break;
          case DXGI_FORMAT_R8G8B8A8_UINT:
            type = -1;  // FIXME type unknown
            mask = 15;
            break;
          case DXGI_FORMAT_R16G16B16A16_SINT:
          case DXGI_FORMAT_R32G32B32A32_SINT:
            type = -1;  // FIXME type unknown
            mask = 15;
            break;
          case DXGI_FORMAT_R8G8B8A8_UNORM:
          case DXGI_FORMAT_R16G16B16A16_SNORM:
          case DXGI_FORMAT_R32G32B32A32_FLOAT:
            type = 3;
            mask = 15;
            break;
          default:
            ASSERT(!"Invalid input element format", return NULL); // NOTREACHED
        }
        bytecode[(0x6C+i*24+12)/4] = type;
        bytecode[(0x6C+i*24+20)/4] = mask;
        const int namesize = strlen(input_elements[i].SemanticName) + 1;
        ASSERT(namesize <= 16, return NULL);
        memcpy(
            (char *)bytecode + 0x6C + D3D11_VS_INPUT_REGISTER_COUNT*24 + i*16,
            input_elements[i].SemanticName, namesize);
    }
    hlsl_checksum(&bytecode[0x14/4], sizeof(bytecode)-0x14, &bytecode[0x4/4]);

#ifdef VERIFY_INPUTLAYOUT_BYTECODE
    ASSERT(le_to_u32(bytecode[0x14/4]) == 1);
    ASSERT(le_to_u32(bytecode[0x18/4]) == sizeof(bytecode));
    ASSERT(le_to_u32(bytecode[0x1C/4]) == 5);
    const int rdef_start = bytecode[0x20/4];
    const int isgn_start = bytecode[0x24/4];
    const int osgn_start = bytecode[0x28/4];
    const int shdr_start = bytecode[0x2C/4];
    const int stat_start = bytecode[0x30/4];
    ASSERT(rdef_start >= 0);
    ASSERT(isgn_start >= 0);
    ASSERT(osgn_start >= 0);
    ASSERT(shdr_start >= 0);
    ASSERT(stat_start >= 0);
    ASSERT(le_to_u32(bytecode[0]) == ('D'|'X'<<8|'B'<<16|'C'<<24));
    ASSERT(le_to_u32(bytecode[rdef_start/4]) == ('R'|'D'<<8|'E'<<16|'F'<<24));
    ASSERT(le_to_u32(bytecode[isgn_start/4]) == ('I'|'S'<<8|'G'<<16|'N'<<24));
    ASSERT(le_to_u32(bytecode[osgn_start/4]) == ('O'|'S'<<8|'G'<<16|'N'<<24));
    ASSERT(le_to_u32(bytecode[shdr_start/4]) == ('S'|'H'<<8|'D'<<16|'R'<<24));
    ASSERT(le_to_u32(bytecode[stat_start/4]) == ('S'|'T'<<8|'A'<<16|'T'<<24));
    ASSERT(rdef_start + 8 + le_to_u32(bytecode[(rdef_start+4)/4])
           <= sizeof(bytecode));
    ASSERT(isgn_start + 8 + le_to_u32(bytecode[(isgn_start+4)/4])
           <= sizeof(bytecode));
    ASSERT(osgn_start + 8 + le_to_u32(bytecode[(osgn_start+4)/4])
           <= sizeof(bytecode));
    ASSERT(shdr_start + 8 + le_to_u32(bytecode[(shdr_start+4)/4])
           <= sizeof(bytecode));
    ASSERT(stat_start + 8 + le_to_u32(bytecode[(stat_start+4)/4])
           <= sizeof(bytecode));

    char shader_buf[4096];
    char *s = shader_buf;
    s += strformat(s, sizeof(shader_buf)-(s-shader_buf), "float4 main(");
    ASSERT(s - shader_buf < (int)sizeof(shader_buf));
    for (int i = 0; i < num_inputs; i++) {
        const char *type;
        switch (input_elements[i].Format) {
          case DXGI_FORMAT_R8_UINT:
            type = "uint";
            break;
          case DXGI_FORMAT_R16_SINT:
          case DXGI_FORMAT_R32_SINT:
            type = "int";
            break;
          case DXGI_FORMAT_R8_UNORM:
          case DXGI_FORMAT_R16_SNORM:
          case DXGI_FORMAT_R32_FLOAT:
            type = "float";
            break;
          case DXGI_FORMAT_R8G8_UINT:
            type = "uint2";
            break;
          case DXGI_FORMAT_R16G16_SINT:
          case DXGI_FORMAT_R32G32_SINT:
            type = "int2";
            break;
          case DXGI_FORMAT_R8G8_UNORM:
          case DXGI_FORMAT_R16G16_SNORM:
          case DXGI_FORMAT_R32G32_FLOAT:
            type = "float2";
            break;
          case DXGI_FORMAT_R32G32B32_SINT:
            type = "int3";
            break;
          case DXGI_FORMAT_R32G32B32_FLOAT:
            type = "float3";
            break;
          case DXGI_FORMAT_R8G8B8A8_UINT:
            type = "uint4";
            break;
          case DXGI_FORMAT_R16G16B16A16_SINT:
          case DXGI_FORMAT_R32G32B32A32_SINT:
            type = "int4";
            break;
          case DXGI_FORMAT_R8G8B8A8_UNORM:
          case DXGI_FORMAT_R16G16B16A16_SNORM:
          case DXGI_FORMAT_R32G32B32A32_FLOAT:
            type = "float4";
            break;
          default:
            ASSERT(!"Invalid input element format", return NULL); // NOTREACHED
        }
        s += strformat(s, sizeof(shader_buf)-(s-shader_buf),
                       "%s%s input%d: %s", i==0 ? "" : ",\n",
                       type, i, input_elements[i].SemanticName);
        ASSERT(s - shader_buf < (int)sizeof(shader_buf));
    }
    s += strformat(s, sizeof(shader_buf)-(s-shader_buf),
                   "): SV_Position {return float4(0.0f,0.0f,0.0f,0.0f);}");
    ASSERT(s - shader_buf < (int)sizeof(shader_buf));
    ID3DBlob *dummy_shader = NULL;
    ASSERT(p_D3DCompile);
    HRESULT compile_result = (*p_D3DCompile)(
        shader_buf, strlen(shader_buf), NULL, NULL, NULL, "main", "vs_4_0",
        D3DCOMPILE_OPTIMIZATION_LEVEL0, 0, &dummy_shader, NULL);
    ASSERT(compile_result == S_OK);
    ASSERT(dummy_shader);
    const uint32_t *d3d_data = ID3D10Blob_GetBufferPointer(dummy_shader);
    const int d3d_size = ID3D10Blob_GetBufferSize(dummy_shader);
    const int d3d_rdef = le_to_s32(d3d_data[0x20/4]);
    const int d3d_isgn = le_to_s32(d3d_data[0x24/4]);
    const int d3d_osgn = le_to_s32(d3d_data[0x28/4]);
    const int d3d_shdr = le_to_s32(d3d_data[0x2C/4]);
    const int d3d_stat = le_to_s32(d3d_data[0x30/4]);
    ASSERT(d3d_rdef >= 0);
    ASSERT(d3d_isgn >= 0);
    ASSERT(d3d_osgn >= 0);
    ASSERT(d3d_shdr >= 0);
    ASSERT(d3d_stat >= 0);

    const char *error = NULL;
    if (memcmp(bytecode, d3d_data, 4) != 0) {
        error = "Header tag mismatch";
    }
    if (memcmp(&bytecode[0x14/4], &d3d_data[0x14/4], 4) != 0) {
        if (!error) {
            error = "Header version mismatch";
        }
    }
    if (memcmp(&bytecode[0x1C/4], &d3d_data[0x1C/4], 4) != 0) {
        if (!error) {
            error = "Header section count mismatch";
        }
    }
    if (memcmp(&bytecode[rdef_start/4], &d3d_data[d3d_rdef/4], 4) != 0) {
        if (!error) {
            error = "RDEF tag mismatch";
        }
    }
    if (memcmp(&bytecode[(rdef_start+8)/4],
               &d3d_data[(d3d_rdef+8)/4], 0x1C) != 0) {
        if (!error) {
            error = "RDEF mismatch";
        }
    }
    if (memcmp(&bytecode[isgn_start/4], &d3d_data[d3d_isgn/4], 4) != 0) {
        if (!error) {
            error = "ISGN tag mismatch";
        }
    }
    if (memcmp(&bytecode[(isgn_start+8)/4],
               &d3d_data[(d3d_isgn+8)/4], 8) != 0) {
        if (!error) {
            error = "ISGN header mismatch";
        }
    }
    char isgn_errorbuf[32];
    for (int i = 0; i < num_inputs; i++) {
        if (memcmp(&bytecode[(isgn_start+20+i*24)/4],
                   &d3d_data[(d3d_isgn+20+i*24)/4], 20) != 0) {
            if (!error) {
                strformat(isgn_errorbuf, sizeof(isgn_errorbuf),
                          "ISGN input %d data mismatch", i);
                error = isgn_errorbuf;
            }
        }
        const int name1 = le_to_s32(bytecode[(isgn_start+16+i*24)/4]);
        const int name2 = le_to_s32(d3d_data[(d3d_isgn+16+i*24)/4]);
        ASSERT(name1 >= 0);
        ASSERT(name2 >= 0);
        if (strcmp(((const char *)bytecode) + (isgn_start+8) + name1,
                   ((const char *)d3d_data) + (d3d_isgn+8) + name2) != 0) {
            if (!error) {
                strformat(isgn_errorbuf, sizeof(isgn_errorbuf),
                          "ISGN input %d name mismatch", i);
                error = isgn_errorbuf;
            }
        }
    }
    if (memcmp(&bytecode[osgn_start/4], &d3d_data[d3d_osgn/4],
               bytecode[(osgn_start+4)/4]) != 0) {
        if (!error) {
            error = "OSGN mismatch";
        }
    }
    if (memcmp(&bytecode[shdr_start/4], &d3d_data[d3d_shdr/4],
               bytecode[(shdr_start+4)/4]) != 0) {
        if (!error) {
            error = "SHDR mismatch";
        }
    }
    /* Work around a difference in the STAT chunk that's only present in
     * d3dcompiler_46.dll. */
    if (d3dcompiler_name
     && strcmp(d3dcompiler_name, "d3dcompiler_46.dll") == 0) {
        ASSERT(bytecode[0x638/4] == 1);
        bytecode[0x638/4] = 2;
    }
    if (memcmp(&bytecode[stat_start/4], &d3d_data[d3d_stat/4],
               bytecode[(stat_start+4)/4]) != 0) {
        if (!error) {
            error = "STAT mismatch";
        }
    }
    if (d3dcompiler_name
     && strcmp(d3dcompiler_name, "d3dcompiler_46.dll") == 0) {
        bytecode[0x638/4] = 1;
    }
    if (error) {
#ifdef DEBUG
        DLOG("InputLayout verification failed: %s", error);
        DLOG("Generated bytecode:");
        for (int i = 0; i < (int)sizeof(bytecode); i += 16) {
            char buf[80];
            s = buf + strformat(buf, sizeof(buf), "%04X:", i);
            for (int j = i; j < i+16; j++) {
                if (j % 4 == 0) {
                    *s++ = ' ';
                }
                if (j < (int)sizeof(bytecode)) {
                    s += strformat(s, sizeof(buf)-(s-buf),
                                   "%02X", ((const uint8_t *)bytecode)[j]);
                } else {
                    s += strformat(s, sizeof(buf)-(s-buf), "  ");
                }
            }
            s += strformat(s, sizeof(buf)-(s-buf), "  ");
            for (int j = i; j < i+16 && j < (int)sizeof(bytecode); j++) {
                uint8_t c = ((const uint8_t *)bytecode)[j];
                if (c >= 0x20 && c <= 0x7E) {
                    *s++ = (char)c;
                } else {
                    *s++ = '.';
                }
            }
            ASSERT(s < buf + sizeof(buf));
            do_DLOG(NULL, 0, 0, "    %s", buf);
        }
        DLOG("Compiled bytecode:");
        for (int i = 0; i < d3d_size; i += 16) {
            char buf[80];
            s = buf + strformat(buf, sizeof(buf), "%04X:", i);
            for (int j = i; j < i+16; j++) {
                if (j % 4 == 0) {
                    *s++ = ' ';
                }
                if (j < d3d_size) {
                    s += strformat(s, sizeof(buf)-(s-buf),
                                   "%02X", ((const uint8_t *)d3d_data)[j]);
                } else {
                    s += strformat(s, sizeof(buf)-(s-buf), "  ");
                }
            }
            s += strformat(s, sizeof(buf)-(s-buf), "  ");
            for (int j = i; j < i+16 && j < d3d_size; j++) {
                uint8_t c = ((const uint8_t *)d3d_data)[j];
                if (c >= 0x20 && c <= 0x7E) {
                    *s++ = (char)c;
                } else {
                    *s++ = '.';
                }
            }
            ASSERT(s < buf + sizeof(buf));
            do_DLOG(NULL, 0, 0, "    %s", buf);
        }
        DLOG("Compiled shader source:");
        for (s = shader_buf; *s; ) {
            char *eol = s + strcspn(s, "\n");
            if (*eol) {
                *eol++ = '\0';
            }
            do_DLOG(NULL, 0, 0, "    %s", s);
            s = eol;
        }
#endif  // DEBUG
        ID3D10Blob_Release(dummy_shader);
        return NULL;
    }

    uint32_t d3d_checksum[4];
    hlsl_checksum(&d3d_data[0x14/4], d3d_size-0x14, d3d_checksum);
    if (memcmp(&d3d_data[0x4/4], d3d_checksum, 16) != 0) {
        DLOG("Checksum mismatch on generated shader (hlsl_checksum() may be"
             "broken)\n      Bytecode: %08X %08X %08X %08X\n    Calculated:"
             " %08X %08X %08X %08X", d3d_data[0x4/4], d3d_data[0x8/4],
              d3d_data[0xC/4], d3d_data[0x10/4], d3d_checksum[0],
             d3d_checksum[1], d3d_checksum[2], d3d_checksum[3]);
        ID3D10Blob_Release(dummy_shader);
        return NULL;
    }

    ID3D10Blob_Release(dummy_shader);
#endif  // VERIFY_INPUTLAYOUT_BYTECODE

    ID3D11InputLayout *input_layout;
    HRESULT result = ID3D11Device_CreateInputLayout(
        d3d_device, input_elements, num_inputs, bytecode, sizeof(bytecode),
        &input_layout);
    if (UNLIKELY(result != S_OK)) {
        DLOG("Failed to create ID3D11InputLayout: %s", d3d_strerror(result));
        return NULL;
    }
    return input_layout;
}

/*-----------------------------------------------------------------------*/

/* Helper function for MD5. */
static inline CONST_FUNCTION uint32_t rol32(uint32_t x, int count) {
    #if defined(__GNUC__)
        /* GCC and Clang lack rotate intrinsics and sometimes have trouble
         * detecting the usual rotate idiom, so use raw assembly to
         * implement the operation. */
        #if defined(__amd64__) || defined(__x86_64__)
            uint32_t result;
            __asm__("rorl %%cl,%0"
                    : "=r" (result) : "0" (x), "c" (32-count));
            return result;
        #elif defined(__mips__)
            uint32_t result;
            __asm__("ror %0,%1,%2" : "=r" (result) : "r" (x), "r" (32-count));
            return result;
        #else
            return x << (count & 31) | x >> (-count & 31);
        #endif
    #elif defined(_MSC_VER)
        return _rotr(x, 32-count);
    #endif
    return x << (count & 31) | x >> (-count & 31);
}

static void hlsl_checksum(const void *data, int size, void *checksum_ret)
{
    PRECOND(data != NULL, return);
    PRECOND(checksum_ret != NULL, return);

    /* The HLSL bytecode checksum algorithm is MD5, but the implementation
     * is slightly incorrect.  (Bug-compatible with old DLLs, perhaps?) */

#ifndef IS_LITTLE_ENDIAN
    #error This function is written for little-endian systems.
    /* Windows currently only runs on little-endian systems, so we don't
     * need to worry about endianness. */
#endif

    static const uint32_t initial_state[4] =
        {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};
    static const uint32_t T[64] = {
        0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A,
        0xA8304613, 0xFD469501, 0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE,
        0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821, 0xF61E2562, 0xC040B340,
        0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
        0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8,
        0x676F02D9, 0x8D2A4C8A, 0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C,
        0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70, 0x289B7EC6, 0xEAA127FA,
        0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
        0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92,
        0xFFEFF47D, 0x85845DD1, 0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1,
        0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391
    };
    /* All macro parameters here are safe without parenthesization. */
    #define F(x,y,z)  ((x & y) | (~x & z))
    #define G(x,y,z)  ((x & z) | (y & ~z))
    #define H(x,y,z)  (x ^ y ^ z)
    #define I(x,y,z)  (y ^ (x | ~z))
    #define ROUND(f,a,b,c,d,k,s,i) \
        (a = b + rol32((a + f(b,c,d) + X[k] + T[i]), s))

    uint32_t *state = checksum_ret;
    memcpy(state, initial_state, 16);

    int padded_size = size;
    if (size % 64 < 56) {
        padded_size += 56 - (size % 64);
    } else {
        padded_size += 112 - (size % 64);
    }
    for (int pos = 0; pos < padded_size; pos += 64) {
        const void *this_data = (const char *)data + pos;
        const uint32_t *X;
        union {
            uint32_t i[16];
            uint8_t b[64];
        } tempbuf;
        if (pos <= size - 64) {
            X = this_data;
        } else if (pos <= size - 56) {
            memcpy(tempbuf.b, this_data, size - pos);
            tempbuf.b[size - pos] = 0x80;
            mem_clear(&tempbuf.b[size - pos + 1], 64 - (size - pos + 1));
            X = tempbuf.i;
        } else {
            /* Proper MD5 would write the final block to the first 56 bytes
             * of the buffer and append a 64-bit length.  This broken
             * version bookends the block with a bit length on one side
             * and a nibble length on the other. */
            if (pos <= size) {
                memcpy(&tempbuf.b[4], this_data, size - pos);
                tempbuf.b[4 + (size - pos)] = 0x80;
                mem_clear(&tempbuf.b[4 + (size - pos + 1)],
                          56 - (size - pos + 1));
            } else {
                mem_clear(&tempbuf.b[4], 56);
            }
            tempbuf.i[0] = size * 8;
            tempbuf.i[15] = (size * 2) | 1;
            X = tempbuf.i;
        }

        uint32_t A = state[0];
        uint32_t B = state[1];
        uint32_t C = state[2];
        uint32_t D = state[3];

        /* Round 1. */
        for (int n = 0; n < 16; n += 4) {
            ROUND(F, A,B,C,D, (n+ 0)&15,  7, n+0);
            ROUND(F, D,A,B,C, (n+ 1)&15, 12, n+1);
            ROUND(F, C,D,A,B, (n+ 2)&15, 17, n+2);
            ROUND(F, B,C,D,A, (n+ 3)&15, 22, n+3);
        }
        /* Round 2. */
        for (int n = 0; n < 16; n += 4) {
            ROUND(G, A,B,C,D, (n+ 1)&15,  5, n+16);
            ROUND(G, D,A,B,C, (n+ 6)&15,  9, n+17);
            ROUND(G, C,D,A,B, (n+11)&15, 14, n+18);
            ROUND(G, B,C,D,A, (n+ 0)&15, 20, n+19);
        }
        /* Round 3. */
        for (int n = 0; n < 16; n += 4) {
            ROUND(H, A,B,C,D, ( 5-n)&15,  4, n+32);
            ROUND(H, D,A,B,C, ( 8-n)&15, 11, n+33);
            ROUND(H, C,D,A,B, (11-n)&15, 16, n+34);
            ROUND(H, B,C,D,A, (14-n)&15, 23, n+35);
        }
        /* Round 4. */
        for (int n = 0; n < 16; n += 4) {
            ROUND(I, A,B,C,D, ( 0-n)&15,  6, n+48);
            ROUND(I, D,A,B,C, ( 7-n)&15, 10, n+49);
            ROUND(I, C,D,A,B, (14-n)&15, 15, n+50);
            ROUND(I, B,C,D,A, ( 5-n)&15, 21, n+51);
        }

        state[0] += A;
        state[1] += B;
        state[2] += C;
        state[3] += D;
    }

    #undef F
    #undef G
    #undef H
    #undef I
    #undef ROUND
}

/*************************************************************************/
/*************************************************************************/
