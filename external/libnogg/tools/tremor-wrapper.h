/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This file is used to redefine symbols exported by the Tremor library
 * so that library can be used alongside the standard libogg and libvorbis
 * libraries.  The Makefile includes this file from the command line when
 * building Tremor sources.
 *
 * This header deliberately does not have a double-inclusion guard since
 * we include it twice from nogg-benchmarks.c, the second time with
 * TREMOR_UNDEF defined to undo all of the renames.
 */

/*************************************************************************/
/*************************************************************************/

/* backends.h */
#define vorbis_func_floor               tremor_vorbis_func_floor
#define vorbis_info_floor0              tremor_vorbis_info_floor0
#define vorbis_info_floor1              tremor_vorbis_info_floor1
#define vorbis_func_residue             tremor_vorbis_func_residue
#define vorbis_info_residue0            tremor_vorbis_info_residue0
#define vorbis_func_mapping             tremor_vorbis_func_mapping
#define vorbis_info_mapping0            tremor_vorbis_info_mapping0

/* block.[ch] */
#define _vorbis_block_ripcord           tremor__vorbis_block_ripcord
#define _vorbis_block_alloc             tremor__vorbis_block_alloc

/* codebook.[ch] */
#define static_codebook                 tremor_static_codebook
#define codebook                        tremor_codebook
#define vorbis_staticbook_destroy       tremor_vorbis_staticbook_destroy
#define vorbis_book_init_decode         tremor_vorbis_book_init_decode
#define vorbis_book_clear               tremor_vorbis_book_clear
#define _book_maptype1_quantvals        tremor__book_maptype1_quantvals
#define vorbis_staticbook_unpack        tremor_vorbis_staticbook_unpack
#define vorbis_book_decode              tremor_vorbis_book_decode
#define vorbis_book_decodevs_add        tremor_vorbis_book_decodevs_add
#define vorbis_book_decodev_set         tremor_vorbis_book_decodev_set
#define vorbis_book_decodev_add         tremor_vorbis_book_decodev_add
#define vorbis_book_decodevv_add        tremor_vorbis_book_decodevv_add
#define _ilog                           tremor__ilog

/* floor0.c */
#define vorbis_lsp_to_curve             tremor_vorbis_lsp_to_curve
#define floor0_exportbundle             tremor_floor0_exportbundle

/* floor1.c */
#define floor1_exportbundle             tremor_floor1_exportbundle

/* ivorbiscodec.h */
#define vorbis_info                     tremor_vorbis_info
#define vorbis_dsp_state                tremor_vorbis_dsp_state
#define vorbis_block                    tremor_vorbis_block
#define alloc_chain                     tremor_alloc_chain
#define vorbis_comment                  tremor_vorbis_comment
#define vorbis_info_init                tremor_vorbis_info_init
#define vorbis_info_clear               tremor_vorbis_info_clear
#define vorbis_info_blocksize           tremor_vorbis_info_blocksize
#define vorbis_comment_init             tremor_vorbis_comment_init
#define vorbis_comment_add              tremor_vorbis_comment_add
#define vorbis_comment_add_tag          tremor_vorbis_comment_add_tag
#define vorbis_comment_query            tremor_vorbis_comment_query
#define vorbis_comment_query_count      tremor_vorbis_comment_query_count
#define vorbis_comment_clear            tremor_vorbis_comment_clear
#define vorbis_block_init               tremor_vorbis_block_init
#define vorbis_block_clear              tremor_vorbis_block_clear
#define vorbis_dsp_clear                tremor_vorbis_dsp_clear
#define vorbis_synthesis_idheader       tremor_vorbis_synthesis_idheader
#define vorbis_synthesis_headerin       tremor_vorbis_synthesis_headerin
#define vorbis_synthesis_init           tremor_vorbis_synthesis_init
#define vorbis_synthesis_restart        tremor_vorbis_synthesis_restart
#define vorbis_synthesis                tremor_vorbis_synthesis
#define vorbis_synthesis_trackonly      tremor_vorbis_synthesis_trackonly
#define vorbis_synthesis_blockin        tremor_vorbis_synthesis_blockin
#define vorbis_synthesis_pcmout         tremor_vorbis_synthesis_pcmout
#define vorbis_synthesis_read           tremor_vorbis_synthesis_read
#define vorbis_packet_blocksize         tremor_vorbis_packet_blocksize

/* ivorbisfile.h */
#define ov_callbacks                    tremor_ov_callbacks
#define OggVorbis_File                  tremor_OggVorbis_File
#define ov_clear                        tremor_ov_clear
#define ov_open                         tremor_ov_open
#define ov_open_callbacks               tremor_ov_open_callbacks
#define ov_fopen                        tremor_ov_fopen
#define ov_test                         tremor_ov_test
#define ov_test_callbacks               tremor_ov_test_callbacks
#define ov_test_open                    tremor_ov_test_open
#define ov_bitrate                      tremor_ov_bitrate
#define ov_bitrate_instant              tremor_ov_bitrate_instant
#define ov_streams                      tremor_ov_streams
#define ov_seekable                     tremor_ov_seekable
#define ov_serialnumber                 tremor_ov_serialnumber
#define ov_raw_total                    tremor_ov_raw_total
#define ov_pcm_total                    tremor_ov_pcm_total
#define ov_time_total                   tremor_ov_time_total
#define ov_raw_seek                     tremor_ov_raw_seek
#define ov_pcm_seek                     tremor_ov_pcm_seek
#define ov_pcm_seek_page                tremor_ov_pcm_seek_page
#define ov_time_seek                    tremor_ov_time_seek
#define ov_time_seek_page               tremor_ov_time_seek_page
#define ov_raw_tell                     tremor_ov_raw_tell
#define ov_pcm_tell                     tremor_ov_pcm_tell
#define ov_time_tell                    tremor_ov_time_tell
#define ov_info                         tremor_ov_info
#define ov_comment                      tremor_ov_comment
#define ov_read                         tremor_ov_read

/* mapping0.c */
#define mapping0_exportbundle           tremor_mapping0_exportbundle

/* mdct.[ch] */
#define mdct_forward                    tremor_mdct_forward
#define mdct_backward                   tremor_mdct_backward

/* misc.h */
#define magic                           tremor_magic

/* registry.[ch] */
#define _floor_P                        tremor__floor_P
#define _residue_P                      tremor__residue_P
#define _mapping_P                      tremor__mapping_P

/* res012.c */
#define res0_free_info                  tremor_res0_free_info
#define res0_free_look                  tremor_res0_free_look
#define res0_unpack                     tremor_res0_unpack
#define res0_look                       tremor_res0_look
#define res0_inverse                    tremor_res0_inverse
#define res1_inverse                    tremor_res1_inverse
#define res2_inverse                    tremor_res2_inverse
#define residue0_exportbundle           tremor_residue0_exportbundle
#define residue1_exportbundle           tremor_residue1_exportbundle
#define residue2_exportbundle           tremor_residue2_exportbundle

/* sharedbook.c */
#define _ilog                           tremor__ilog
#define _make_words                     tremor__make_words
#define _book_maptype1_quantvals        tremor__book_maptype1_quantvals
#define _book_unquantize                tremor__book_unquantize

/* window.[ch] */
#define _vorbis_window                  tremor__vorbis_window
#define _vorbis_apply_window            tremor__vorbis_apply_window

/*-----------------------------------------------------------------------*/

#ifdef TREMOR_UNDEF

#undef vorbis_func_floor
#undef vorbis_info_floor0
#undef vorbis_info_floor1
#undef vorbis_func_residue
#undef vorbis_info_residue0
#undef vorbis_func_mapping
#undef vorbis_info_mapping0
#undef _vorbis_block_ripcord
#undef _vorbis_block_alloc
#undef static_codebook
#undef codebook
#undef vorbis_staticbook_destroy
#undef vorbis_book_init_decode
#undef vorbis_book_clear
#undef _book_maptype1_quantvals
#undef vorbis_staticbook_unpack
#undef vorbis_book_decode
#undef vorbis_book_decodevs_add
#undef vorbis_book_decodev_set
#undef vorbis_book_decodev_add
#undef vorbis_book_decodevv_add
#undef _ilog
#undef vorbis_lsp_to_curve
#undef floor0_exportbundle
#undef floor1_exportbundle
#undef vorbis_info
#undef vorbis_dsp_state
#undef vorbis_block
#undef vorbis_comment
#undef vorbis_info_init
#undef vorbis_info_clear
#undef vorbis_info_blocksize
#undef vorbis_comment_init
#undef vorbis_comment_add
#undef vorbis_comment_add_tag
#undef vorbis_comment_query
#undef vorbis_comment_query_count
#undef vorbis_comment_clear
#undef vorbis_block_init
#undef vorbis_block_clear
#undef vorbis_dsp_clear
#undef vorbis_synthesis_idheader
#undef vorbis_synthesis_headerin
#undef vorbis_synthesis_init
#undef vorbis_synthesis_restart
#undef vorbis_synthesis
#undef vorbis_synthesis_trackonly
#undef vorbis_synthesis_blockin
#undef vorbis_synthesis_pcmout
#undef vorbis_synthesis_read
#undef vorbis_packet_blocksize
#undef ov_callbacks
#undef OggVorbis_File
#undef ov_clear
#undef ov_open
#undef ov_open_callbacks
#undef ov_fopen
#undef ov_test
#undef ov_test_callbacks
#undef ov_test_open
#undef ov_bitrate
#undef ov_bitrate_instant
#undef ov_streams
#undef ov_seekable
#undef ov_serialnumber
#undef ov_raw_total
#undef ov_pcm_total
#undef ov_time_total
#undef ov_raw_seek
#undef ov_pcm_seek
#undef ov_pcm_seek_page
#undef ov_time_seek
#undef ov_time_seek_page
#undef ov_raw_tell
#undef ov_pcm_tell
#undef ov_time_tell
#undef ov_info
#undef ov_comment
#undef ov_read
#undef mapping0_exportbundle
#undef mdct_forward
#undef mdct_backward
#undef magic
#undef _floor_P
#undef _residue_P
#undef _mapping_P
#undef res0_free_info
#undef res0_free_look
#undef res0_unpack
#undef res0_look
#undef res0_inverse
#undef res1_inverse
#undef res2_inverse
#undef residue0_exportbundle
#undef residue1_exportbundle
#undef residue2_exportbundle
#undef _vorbis_window
#undef _vorbis_apply_window

#endif  // TREMOR_UNDEF

/*************************************************************************/
/*************************************************************************/
