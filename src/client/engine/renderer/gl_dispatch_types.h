#ifndef CODUOMP_RENDERER_GL_DISPATCH_TYPES_H
#define CODUOMP_RENDERER_GL_DISPATCH_TYPES_H

#include "gl_sdk.h"

#if defined(_WIN32)
#define RENDERER_GL_API_CALL APIENTRY
#else
#define RENDERER_GL_API_CALL
#endif

/* The SDK owns parameter types, return types and calling conventions. Taking
 * a function's type does not link it: QGL still resolves every slot at runtime.
 * Apple's SDK omits several legacy ATI/NV/EXT interfaces; only those aliases
 * retain explicit declarations, checked against the Khronos extension ABI. */
typedef __typeof__(&glAlphaFunc) renderer_gl_alpha_func_t;
typedef __typeof__(&glBegin) renderer_gl_begin_func_t;
typedef __typeof__(&glBindTexture) renderer_gl_bind_texture_func_t;
typedef __typeof__(&glBindProgramARB) renderer_gl_bind_program_arb_func_t;
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_bind_fragment_shader_ati_func_t)(
    uint32_t shader);
#else
typedef __typeof__(&glBindFragmentShaderATI) renderer_gl_bind_fragment_shader_ati_func_t;
#endif
typedef __typeof__(&glBindBufferARB) renderer_gl_bind_buffer_arb_func_t;
typedef __typeof__(&glBufferDataARB) renderer_gl_buffer_data_arb_func_t;
typedef __typeof__(&glMapBufferARB) renderer_gl_map_buffer_arb_func_t;
typedef __typeof__(&glUnmapBufferARB) renderer_gl_unmap_buffer_arb_func_t;
typedef __typeof__(&glBlendFunc) renderer_gl_blend_func_t;
typedef __typeof__(&glCallList) renderer_gl_call_list_func_t;
typedef __typeof__(&glClear) renderer_gl_clear_func_t;
typedef __typeof__(&glClearDepth) renderer_gl_clear_depth_func_t;
typedef __typeof__(&glClearStencil) renderer_gl_clear_stencil_func_t;
typedef __typeof__(&glColor4f) renderer_gl_color4f_func_t;
typedef __typeof__(&glColor4fv) renderer_gl_color4fv_func_t;
typedef __typeof__(&glColorPointer) renderer_gl_color_pointer_func_t;
typedef __typeof__(&glCullFace) renderer_gl_cull_face_func_t;
typedef __typeof__(&glDepthFunc) renderer_gl_depth_func_t;
typedef __typeof__(&glDepthMask) renderer_gl_depth_mask_func_t;
typedef __typeof__(&glDepthRange) renderer_gl_depth_range_func_t;
typedef __typeof__(&glDisable) renderer_gl_capability_func_t;
typedef __typeof__(&glDrawElements) renderer_gl_draw_elements_func_t;
typedef __typeof__(&glDrawRangeElementsEXT) renderer_gl_draw_range_elements_func_t;
typedef __typeof__(&glDrawBuffer) renderer_gl_draw_buffer_func_t;
typedef __typeof__(&glHint) renderer_gl_hint_func_t;
typedef __typeof__(&glFogf) renderer_gl_fogf_func_t;
typedef __typeof__(&glFogfv) renderer_gl_fogfv_func_t;
typedef __typeof__(&glFogi) renderer_gl_fogi_func_t;
typedef __typeof__(&glPolygonMode) renderer_gl_polygon_mode_func_t;
typedef __typeof__(&glScissor) renderer_gl_scissor_func_t;
typedef __typeof__(&glTexCoordPointer) renderer_gl_tex_coord_pointer_func_t;
typedef __typeof__(&glTexEnvf) renderer_gl_tex_envf_func_t;
typedef __typeof__(&glTexEnvi) renderer_gl_tex_envi_func_t;
typedef __typeof__(&glTexParameterf) renderer_gl_tex_parameterf_func_t;
typedef __typeof__(&glTexParameteri) renderer_gl_tex_parameteri_func_t;
typedef __typeof__(&glNormalPointer) renderer_gl_normal_pointer_func_t;
typedef __typeof__(&glVertexPointer) renderer_gl_vertex_pointer_func_t;
typedef __typeof__(&glViewport) renderer_gl_viewport_func_t;
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_set_fence_nv_func_t)(
    uint32_t fence, uint32_t condition);
#else
typedef __typeof__(&glSetFenceNV) renderer_gl_set_fence_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_finish_fence_nv_func_t)(
    uint32_t fence);
#else
typedef __typeof__(&glFinishFenceNV) renderer_gl_finish_fence_nv_func_t;
#endif
typedef __typeof__(&glActiveTextureARB) renderer_gl_active_texture_arb_func_t;
typedef __typeof__(&glMultiTexCoord2fARB) renderer_gl_multi_tex_coord2f_arb_func_t;
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_lock_arrays_ext_func_t)(
    int32_t first, int32_t count);
#else
typedef __typeof__(&glLockArraysEXT) renderer_gl_lock_arrays_ext_func_t;
#endif
typedef __typeof__(&glEnd) renderer_gl_void_func_t;
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_pn_trianglesi_ati_func_t)(
    uint32_t parameter, int32_t value);
#else
typedef __typeof__(&glPNTrianglesiATI) renderer_gl_pn_trianglesi_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_pn_trianglesf_ati_func_t)(
    uint32_t parameter, float value);
#else
typedef __typeof__(&glPNTrianglesfATI) renderer_gl_pn_trianglesf_ati_func_t;
#endif
typedef __typeof__(&glCompressedTexImage3DARB) renderer_gl_compressed_tex_image_3d_func_t;
typedef __typeof__(&glCompressedTexImage2DARB) renderer_gl_compressed_tex_image_2d_func_t;
typedef __typeof__(&glCompressedTexImage1DARB) renderer_gl_compressed_tex_image_1d_func_t;
typedef __typeof__(&glCompressedTexSubImage3DARB) renderer_gl_compressed_tex_sub_image_3d_func_t;
typedef __typeof__(&glCompressedTexSubImage2DARB) renderer_gl_compressed_tex_sub_image_2d_func_t;
typedef __typeof__(&glCompressedTexSubImage1DARB) renderer_gl_compressed_tex_sub_image_1d_func_t;
typedef __typeof__(&glGetCompressedTexImageARB) renderer_gl_get_compressed_tex_image_func_t;
typedef __typeof__(&glDeleteBuffersARB) renderer_gl_delete_buffers_arb_func_t;
typedef __typeof__(&glGenBuffersARB) renderer_gl_gen_buffers_arb_func_t;
typedef __typeof__(&glIsBufferARB) renderer_gl_is_buffer_arb_func_t;
typedef __typeof__(&glBufferSubDataARB) renderer_gl_buffer_sub_data_arb_func_t;
typedef __typeof__(&glGetBufferSubDataARB) renderer_gl_get_buffer_sub_data_arb_func_t;
typedef __typeof__(&glGetBufferParameterivARB) renderer_gl_get_buffer_parameteriv_arb_func_t;
typedef __typeof__(&glGetBufferPointervARB) renderer_gl_get_buffer_pointerv_arb_func_t;
#if defined(__APPLE__)
typedef uint32_t (RENDERER_GL_API_CALL
                  *renderer_gl_new_object_buffer_ati_func_t)(
    int32_t size, const void *data, uint32_t usage);
#else
typedef __typeof__(&glNewObjectBufferATI) renderer_gl_new_object_buffer_ati_func_t;
#endif
#if defined(__APPLE__)
typedef uint8_t (RENDERER_GL_API_CALL
                 *renderer_gl_is_object_buffer_ati_func_t)(uint32_t buffer);
#else
typedef __typeof__(&glIsObjectBufferATI) renderer_gl_is_object_buffer_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_update_object_buffer_ati_func_t)(
    uint32_t buffer, uint32_t offset, int32_t size, const void *data,
    uint32_t preserveMode);
#else
typedef __typeof__(&glUpdateObjectBufferATI) renderer_gl_update_object_buffer_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_object_bufferfv_ati_func_t)(
    uint32_t buffer, uint32_t parameter, float *values);
#else
typedef __typeof__(&glGetObjectBufferfvATI) renderer_gl_get_object_bufferfv_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_object_bufferiv_ati_func_t)(
    uint32_t buffer, uint32_t parameter, int32_t *values);
#else
typedef __typeof__(&glGetObjectBufferivATI) renderer_gl_get_object_bufferiv_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_free_object_buffer_ati_func_t)(
    uint32_t buffer);
#else
typedef __typeof__(&glFreeObjectBufferATI) renderer_gl_free_object_buffer_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_array_object_ati_func_t)(
    uint32_t array, int32_t size, uint32_t type, int32_t stride,
    uint32_t buffer, uint32_t offset);
#else
typedef __typeof__(&glArrayObjectATI) renderer_gl_array_object_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_array_objectfv_ati_func_t)(
    uint32_t array, uint32_t parameter, float *values);
#else
typedef __typeof__(&glGetArrayObjectfvATI) renderer_gl_get_array_objectfv_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_array_objectiv_ati_func_t)(
    uint32_t array, uint32_t parameter, int32_t *values);
#else
typedef __typeof__(&glGetArrayObjectivATI) renderer_gl_get_array_objectiv_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_variant_array_object_ati_func_t)(
    uint32_t id, uint32_t type, int32_t stride, uint32_t buffer,
    uint32_t offset);
#else
typedef __typeof__(&glVariantArrayObjectATI) renderer_gl_variant_array_object_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_variant_array_objectfv_ati_func_t)(
    uint32_t id, uint32_t parameter, float *values);
#else
typedef __typeof__(&glGetVariantArrayObjectfvATI) renderer_gl_get_variant_array_objectfv_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_variant_array_objectiv_ati_func_t)(
    uint32_t id, uint32_t parameter, int32_t *values);
#else
typedef __typeof__(&glGetVariantArrayObjectivATI) renderer_gl_get_variant_array_objectiv_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_element_pointer_ati_func_t)(
    uint32_t type, const void *pointer);
#else
typedef __typeof__(&glElementPointerATI) renderer_gl_element_pointer_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_draw_element_array_ati_func_t)(
    uint32_t mode, int32_t count);
#else
typedef __typeof__(&glDrawElementArrayATI) renderer_gl_draw_element_array_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_draw_range_element_array_ati_func_t)(
    uint32_t mode, uint32_t start, uint32_t end, int32_t count);
#else
typedef __typeof__(&glDrawRangeElementArrayATI) renderer_gl_draw_range_element_array_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_vertex_array_range_nv_func_t)(
    int32_t length, const void *pointer);
#else
typedef __typeof__(&glVertexArrayRangeNV) renderer_gl_vertex_array_range_nv_func_t;
#endif

/* These two GL dispatch slots resolve WGL_NV_vertex_array_range exports. */
#if defined(_WIN32)
typedef PFNWGLALLOCATEMEMORYNVPROC renderer_gl_allocate_memory_nv_func_t;
#else
typedef void *(RENDERER_GL_API_CALL
               *renderer_gl_allocate_memory_nv_func_t)(
    int32_t size, float readFrequency, float writeFrequency, float priority);
#endif
#if defined(_WIN32)
typedef PFNWGLFREEMEMORYNVPROC renderer_gl_free_memory_nv_func_t;
#else
typedef void (RENDERER_GL_API_CALL *renderer_gl_free_memory_nv_func_t)(
    void *memory);
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_delete_fences_nv_func_t)(
    int32_t count, const uint32_t *fences);
#else
typedef __typeof__(&glDeleteFencesNV) renderer_gl_delete_fences_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_gen_fences_nv_func_t)(
    int32_t count, uint32_t *fences);
#else
typedef __typeof__(&glGenFencesNV) renderer_gl_gen_fences_nv_func_t;
#endif
#if defined(__APPLE__)
typedef uint8_t (RENDERER_GL_API_CALL *renderer_gl_fence_test_nv_func_t)(
    uint32_t fence);
#else
typedef __typeof__(&glIsFenceNV) renderer_gl_fence_test_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_get_fenceiv_nv_func_t)(
    uint32_t fence, uint32_t parameter, int32_t *values);
#else
typedef __typeof__(&glGetFenceivNV) renderer_gl_get_fenceiv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_combiner_parameterfv_nv_func_t)(
    uint32_t parameter, const float *values);
#else
typedef __typeof__(&glCombinerParameterfvNV) renderer_gl_combiner_parameterfv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_combiner_parameterf_nv_func_t)(
    uint32_t parameter, float value);
#else
typedef __typeof__(&glCombinerParameterfNV) renderer_gl_combiner_parameterf_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_combiner_parameteriv_nv_func_t)(
    uint32_t parameter, const int32_t *values);
#else
typedef __typeof__(&glCombinerParameterivNV) renderer_gl_combiner_parameteriv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_combiner_parameteri_nv_func_t)(
    uint32_t parameter, int32_t value);
#else
typedef __typeof__(&glCombinerParameteriNV) renderer_gl_combiner_parameteri_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_combiner_input_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t variable, uint32_t input,
    uint32_t mapping, uint32_t componentUsage);
#else
typedef __typeof__(&glCombinerInputNV) renderer_gl_combiner_input_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL *renderer_gl_combiner_output_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t abOutput, uint32_t cdOutput,
    uint32_t sumOutput, uint32_t scale, uint32_t bias,
    uint8_t abDotProduct, uint8_t cdDotProduct, uint8_t muxSum);
#else
typedef __typeof__(&glCombinerOutputNV) renderer_gl_combiner_output_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_final_combiner_input_nv_func_t)(
    uint32_t variable, uint32_t input, uint32_t mapping,
    uint32_t componentUsage);
#else
typedef __typeof__(&glFinalCombinerInputNV) renderer_gl_final_combiner_input_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_combiner_input_parameterfv_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
    float *values);
#else
typedef __typeof__(&glGetCombinerInputParameterfvNV) renderer_gl_get_combiner_input_parameterfv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_combiner_input_parameteriv_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t variable, uint32_t parameter,
    int32_t *values);
#else
typedef __typeof__(&glGetCombinerInputParameterivNV) renderer_gl_get_combiner_input_parameteriv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_combiner_output_parameterfv_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t parameter, float *values);
#else
typedef __typeof__(&glGetCombinerOutputParameterfvNV) renderer_gl_get_combiner_output_parameterfv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_combiner_output_parameteriv_nv_func_t)(
    uint32_t stage, uint32_t portion, uint32_t parameter, int32_t *values);
#else
typedef __typeof__(&glGetCombinerOutputParameterivNV) renderer_gl_get_combiner_output_parameteriv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_final_combiner_input_parameterfv_nv_func_t)(
    uint32_t variable, uint32_t parameter, float *values);
#else
typedef __typeof__(&glGetFinalCombinerInputParameterfvNV) renderer_gl_get_final_combiner_input_parameterfv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_final_combiner_input_parameteriv_nv_func_t)(
    uint32_t variable, uint32_t parameter, int32_t *values);
#else
typedef __typeof__(&glGetFinalCombinerInputParameterivNV) renderer_gl_get_final_combiner_input_parameteriv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_combiner_stage_parameterfv_nv_func_t)(
    uint32_t stage, uint32_t parameter, const float *values);
#else
typedef __typeof__(&glCombinerStageParameterfvNV) renderer_gl_combiner_stage_parameterfv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_get_combiner_stage_parameterfv_nv_func_t)(
    uint32_t stage, uint32_t parameter, float *values);
#else
typedef __typeof__(&glGetCombinerStageParameterfvNV) renderer_gl_get_combiner_stage_parameterfv_nv_func_t;
#endif
#if defined(__APPLE__)
typedef uint32_t (RENDERER_GL_API_CALL
                  *renderer_gl_gen_fragment_shaders_ati_func_t)(
    uint32_t range);
#else
typedef __typeof__(&glGenFragmentShadersATI) renderer_gl_gen_fragment_shaders_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_fragment_shader_texcoord_ati_func_t)(
    uint32_t destination, uint32_t coordinate, uint32_t swizzle);
#else
typedef __typeof__(&glPassTexCoordATI) renderer_gl_fragment_shader_texcoord_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_color_fragment_op1_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationMask,
    uint32_t destinationModifier, uint32_t argument1,
    uint32_t argument1Replication, uint32_t argument1Modifier);
#else
typedef __typeof__(&glColorFragmentOp1ATI) renderer_gl_color_fragment_op1_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_color_fragment_op2_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationMask,
    uint32_t destinationModifier, uint32_t argument1,
    uint32_t argument1Replication, uint32_t argument1Modifier,
    uint32_t argument2, uint32_t argument2Replication,
    uint32_t argument2Modifier);
#else
typedef __typeof__(&glColorFragmentOp2ATI) renderer_gl_color_fragment_op2_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_color_fragment_op3_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationMask,
    uint32_t destinationModifier, uint32_t argument1,
    uint32_t argument1Replication, uint32_t argument1Modifier,
    uint32_t argument2, uint32_t argument2Replication,
    uint32_t argument2Modifier, uint32_t argument3,
    uint32_t argument3Replication, uint32_t argument3Modifier);
#else
typedef __typeof__(&glColorFragmentOp3ATI) renderer_gl_color_fragment_op3_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_alpha_fragment_op1_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationModifier,
    uint32_t argument1, uint32_t argument1Replication,
    uint32_t argument1Modifier);
#else
typedef __typeof__(&glAlphaFragmentOp1ATI) renderer_gl_alpha_fragment_op1_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_alpha_fragment_op2_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationModifier,
    uint32_t argument1, uint32_t argument1Replication,
    uint32_t argument1Modifier, uint32_t argument2,
    uint32_t argument2Replication, uint32_t argument2Modifier);
#else
typedef __typeof__(&glAlphaFragmentOp2ATI) renderer_gl_alpha_fragment_op2_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_alpha_fragment_op3_ati_func_t)(
    uint32_t operation, uint32_t destination, uint32_t destinationModifier,
    uint32_t argument1, uint32_t argument1Replication,
    uint32_t argument1Modifier, uint32_t argument2,
    uint32_t argument2Replication, uint32_t argument2Modifier,
    uint32_t argument3, uint32_t argument3Replication,
    uint32_t argument3Modifier);
#else
typedef __typeof__(&glAlphaFragmentOp3ATI) renderer_gl_alpha_fragment_op3_ati_func_t;
#endif
#if defined(__APPLE__)
typedef void (RENDERER_GL_API_CALL
              *renderer_gl_set_fragment_shader_constant_ati_func_t)(
    uint32_t destination, const float *value);
#else
typedef __typeof__(&glSetFragmentShaderConstantATI) renderer_gl_set_fragment_shader_constant_ati_func_t;
#endif
typedef __typeof__(&glVertexAttrib1sARB) renderer_gl_vertex_attrib1s_arb_func_t;
typedef __typeof__(&glVertexAttrib1fARB) renderer_gl_vertex_attrib1f_arb_func_t;
typedef __typeof__(&glVertexAttrib1dARB) renderer_gl_vertex_attrib1d_arb_func_t;
typedef __typeof__(&glVertexAttrib2sARB) renderer_gl_vertex_attrib2s_arb_func_t;
typedef __typeof__(&glVertexAttrib2fARB) renderer_gl_vertex_attrib2f_arb_func_t;
typedef __typeof__(&glVertexAttrib2dARB) renderer_gl_vertex_attrib2d_arb_func_t;
typedef __typeof__(&glVertexAttrib3sARB) renderer_gl_vertex_attrib3s_arb_func_t;
typedef __typeof__(&glVertexAttrib3fARB) renderer_gl_vertex_attrib3f_arb_func_t;
typedef __typeof__(&glVertexAttrib3dARB) renderer_gl_vertex_attrib3d_arb_func_t;
typedef __typeof__(&glVertexAttrib4sARB) renderer_gl_vertex_attrib4s_arb_func_t;
typedef __typeof__(&glVertexAttrib4fARB) renderer_gl_vertex_attrib4f_arb_func_t;
typedef __typeof__(&glVertexAttrib4dARB) renderer_gl_vertex_attrib4d_arb_func_t;
typedef __typeof__(&glVertexAttrib4NubARB) renderer_gl_vertex_attrib4nub_arb_func_t;
typedef __typeof__(&glVertexAttrib4bvARB) renderer_gl_vertex_attrib_i8v_arb_func_t;
typedef __typeof__(&glVertexAttrib1svARB) renderer_gl_vertex_attrib_i16v_arb_func_t;
typedef __typeof__(&glVertexAttrib4ivARB) renderer_gl_vertex_attrib_i32v_arb_func_t;
typedef __typeof__(&glVertexAttrib4ubvARB) renderer_gl_vertex_attrib_u8v_arb_func_t;
typedef __typeof__(&glVertexAttrib4usvARB) renderer_gl_vertex_attrib_u16v_arb_func_t;
typedef __typeof__(&glVertexAttrib4uivARB) renderer_gl_vertex_attrib_u32v_arb_func_t;
typedef __typeof__(&glVertexAttrib1fvARB) renderer_gl_vertex_attrib_fv_arb_func_t;
typedef __typeof__(&glVertexAttrib1dvARB) renderer_gl_vertex_attrib_dv_arb_func_t;
typedef __typeof__(&glVertexAttribPointerARB) renderer_gl_vertex_attrib_pointer_arb_func_t;
typedef __typeof__(&glEnableVertexAttribArrayARB) renderer_gl_vertex_attrib_array_arb_func_t;
typedef __typeof__(&glProgramStringARB) renderer_gl_program_string_arb_func_t;
typedef __typeof__(&glDeleteProgramsARB) renderer_gl_delete_programs_arb_func_t;
typedef __typeof__(&glGenProgramsARB) renderer_gl_gen_programs_arb_func_t;
typedef __typeof__(&glProgramEnvParameter4fARB) renderer_gl_program_parameter4f_arb_func_t;
typedef __typeof__(&glProgramEnvParameter4dARB) renderer_gl_program_parameter4d_arb_func_t;
typedef __typeof__(&glProgramEnvParameter4fvARB) renderer_gl_program_parameterfv_arb_func_t;
typedef __typeof__(&glProgramEnvParameter4dvARB) renderer_gl_program_parameterdv_arb_func_t;
typedef __typeof__(&glGetProgramEnvParameterfvARB) renderer_gl_get_program_parameterfv_arb_func_t;
typedef __typeof__(&glGetProgramEnvParameterdvARB) renderer_gl_get_program_parameterdv_arb_func_t;
typedef __typeof__(&glGetProgramivARB) renderer_gl_get_programiv_arb_func_t;
typedef __typeof__(&glGetProgramStringARB) renderer_gl_get_program_string_arb_func_t;
typedef __typeof__(&glGetVertexAttribdvARB) renderer_gl_get_vertex_attribdv_arb_func_t;
typedef __typeof__(&glGetVertexAttribfvARB) renderer_gl_get_vertex_attribfv_arb_func_t;
typedef __typeof__(&glGetVertexAttribivARB) renderer_gl_get_vertex_attribiv_arb_func_t;
typedef __typeof__(&glGetVertexAttribPointervARB) renderer_gl_get_vertex_attrib_pointerv_arb_func_t;
typedef __typeof__(&glIsProgramARB) renderer_gl_is_program_arb_func_t;
typedef __typeof__(&glAccum) renderer_gl_accum_func_t;
typedef __typeof__(&glAreTexturesResident) renderer_gl_are_textures_resident_func_t;
typedef __typeof__(&glArrayElement) renderer_gl_array_element_func_t;
typedef __typeof__(&glBitmap) renderer_gl_bitmap_func_t;
typedef __typeof__(&glCallLists) renderer_gl_call_lists_func_t;
typedef __typeof__(&glClearAccum) renderer_gl_clear_color4_func_t;
typedef __typeof__(&glClearIndex) renderer_gl_clear_index_func_t;
typedef __typeof__(&glClipPlane) renderer_gl_clip_plane_func_t;
typedef __typeof__(&glColor3b) renderer_gl_color3b_func_t;
typedef __typeof__(&glColor3bv) renderer_gl_color3bv_func_t;
typedef __typeof__(&glColor3d) renderer_gl_color3d_func_t;
typedef __typeof__(&glColor3dv) renderer_gl_color3dv_func_t;
typedef __typeof__(&glColor3f) renderer_gl_color3f_func_t;
typedef __typeof__(&glColor3fv) renderer_gl_color3fv_func_t;
typedef __typeof__(&glColor3i) renderer_gl_color3i_func_t;
typedef __typeof__(&glColor3iv) renderer_gl_color3iv_func_t;
typedef __typeof__(&glColor3s) renderer_gl_color3s_func_t;
typedef __typeof__(&glColor3sv) renderer_gl_color3sv_func_t;
typedef __typeof__(&glColor3ub) renderer_gl_color3ub_func_t;
typedef __typeof__(&glColor3ubv) renderer_gl_color3ubv_func_t;
typedef __typeof__(&glColor3ui) renderer_gl_color3ui_func_t;
typedef __typeof__(&glColor3uiv) renderer_gl_color3uiv_func_t;
typedef __typeof__(&glColor3us) renderer_gl_color3us_func_t;
typedef __typeof__(&glColor3usv) renderer_gl_color3usv_func_t;
typedef __typeof__(&glColor4b) renderer_gl_color4b_func_t;
typedef __typeof__(&glColor4bv) renderer_gl_color4bv_func_t;
typedef __typeof__(&glColor4d) renderer_gl_color4d_func_t;
typedef __typeof__(&glColor4dv) renderer_gl_color4dv_func_t;
typedef __typeof__(&glColor4i) renderer_gl_color4i_func_t;
typedef __typeof__(&glColor4iv) renderer_gl_color4iv_func_t;
typedef __typeof__(&glColor4s) renderer_gl_color4s_func_t;
typedef __typeof__(&glColor4sv) renderer_gl_color4sv_func_t;
typedef __typeof__(&glColor4ub) renderer_gl_color4ub_func_t;
typedef __typeof__(&glColor4ubv) renderer_gl_color4ubv_func_t;
typedef __typeof__(&glColor4ui) renderer_gl_color4ui_func_t;
typedef __typeof__(&glColor4uiv) renderer_gl_color4uiv_func_t;
typedef __typeof__(&glColor4us) renderer_gl_color4us_func_t;
typedef __typeof__(&glColor4usv) renderer_gl_color4usv_func_t;
typedef __typeof__(&glColorMask) renderer_gl_color_mask_func_t;
typedef __typeof__(&glColorMaterial) renderer_gl_color_material_func_t;
typedef __typeof__(&glCopyPixels) renderer_gl_copy_pixels_func_t;
typedef __typeof__(&glCopyTexImage1D) renderer_gl_copy_tex_image_1d_func_t;
typedef __typeof__(&glCopyTexImage2D) renderer_gl_copy_tex_image_2d_func_t;
typedef __typeof__(&glCopyTexSubImage1D) renderer_gl_copy_tex_sub_image_1d_func_t;
typedef __typeof__(&glCopyTexSubImage2D) renderer_gl_copy_tex_sub_image_2d_func_t;
typedef __typeof__(&glDeleteLists) renderer_gl_delete_lists_func_t;
typedef __typeof__(&glDeleteTextures) renderer_gl_delete_textures_func_t;
typedef __typeof__(&glDrawArrays) renderer_gl_draw_arrays_func_t;
typedef __typeof__(&glDrawPixels) renderer_gl_draw_pixels_func_t;
typedef __typeof__(&glEdgeFlag) renderer_gl_edge_flag_func_t;
typedef __typeof__(&glEdgeFlagPointer) renderer_gl_edge_flag_pointer_func_t;
typedef __typeof__(&glEdgeFlagv) renderer_gl_edge_flagv_func_t;
typedef __typeof__(&glEvalCoord1d) renderer_gl_eval_coord1d_func_t;
typedef __typeof__(&glEvalCoord1dv) renderer_gl_eval_coord1dv_func_t;
typedef __typeof__(&glEvalCoord1f) renderer_gl_eval_coord1f_func_t;
typedef __typeof__(&glEvalCoord1fv) renderer_gl_eval_coord1fv_func_t;
typedef __typeof__(&glEvalCoord2d) renderer_gl_eval_coord2d_func_t;
typedef __typeof__(&glEvalCoord2dv) renderer_gl_eval_coord2dv_func_t;
typedef __typeof__(&glEvalCoord2f) renderer_gl_eval_coord2f_func_t;
typedef __typeof__(&glEvalCoord2fv) renderer_gl_eval_coord2fv_func_t;
typedef __typeof__(&glEvalMesh1) renderer_gl_eval_mesh1_func_t;
typedef __typeof__(&glEvalMesh2) renderer_gl_eval_mesh2_func_t;
typedef __typeof__(&glEvalPoint1) renderer_gl_eval_point1_func_t;
typedef __typeof__(&glEvalPoint2) renderer_gl_eval_point2_func_t;
typedef __typeof__(&glFeedbackBuffer) renderer_gl_feedback_buffer_func_t;
typedef __typeof__(&glFogiv) renderer_gl_fogiv_func_t;
typedef __typeof__(&glFrontFace) renderer_gl_front_face_func_t;
typedef __typeof__(&glFrustum) renderer_gl_frustum_func_t;
typedef __typeof__(&glGenLists) renderer_gl_gen_lists_func_t;
typedef __typeof__(&glGenTextures) renderer_gl_gen_textures_func_t;
typedef __typeof__(&glGetBooleanv) renderer_gl_get_booleanv_func_t;
typedef __typeof__(&glGetClipPlane) renderer_gl_get_clip_plane_func_t;
typedef __typeof__(&glGetDoublev) renderer_gl_get_doublev_func_t;
typedef __typeof__(&glGetError) renderer_gl_get_error_func_t;
typedef __typeof__(&glGetFloatv) renderer_gl_get_floatv_func_t;
typedef __typeof__(&glGetIntegerv) renderer_gl_get_integerv_func_t;
typedef __typeof__(&glGetLightfv) renderer_gl_get_lightfv_func_t;
typedef __typeof__(&glGetLightiv) renderer_gl_get_lightiv_func_t;
typedef __typeof__(&glGetMapdv) renderer_gl_get_mapdv_func_t;
typedef __typeof__(&glGetMapfv) renderer_gl_get_mapfv_func_t;
typedef __typeof__(&glGetMapiv) renderer_gl_get_mapiv_func_t;
typedef __typeof__(&glGetMaterialfv) renderer_gl_get_materialfv_func_t;
typedef __typeof__(&glGetMaterialiv) renderer_gl_get_materialiv_func_t;
typedef __typeof__(&glGetPixelMapfv) renderer_gl_get_pixel_mapfv_func_t;
typedef __typeof__(&glGetPixelMapuiv) renderer_gl_get_pixel_mapuiv_func_t;
typedef __typeof__(&glGetPixelMapusv) renderer_gl_get_pixel_mapusv_func_t;
typedef __typeof__(&glGetPointerv) renderer_gl_get_pointerv_func_t;
typedef __typeof__(&glGetPolygonStipple) renderer_gl_get_polygon_stipple_func_t;
typedef __typeof__(&glGetString) renderer_gl_get_string_func_t;
typedef __typeof__(&glGetTexEnvfv) renderer_gl_get_tex_envfv_func_t;
typedef __typeof__(&glGetTexEnviv) renderer_gl_get_tex_enviv_func_t;
typedef __typeof__(&glGetTexGendv) renderer_gl_get_tex_gendv_func_t;
typedef __typeof__(&glGetTexGenfv) renderer_gl_get_tex_genfv_func_t;
typedef __typeof__(&glGetTexGeniv) renderer_gl_get_tex_geniv_func_t;
typedef __typeof__(&glGetTexImage) renderer_gl_get_tex_image_func_t;
typedef __typeof__(&glGetTexLevelParameterfv) renderer_gl_get_tex_level_parameterfv_func_t;
typedef __typeof__(&glGetTexLevelParameteriv) renderer_gl_get_tex_level_parameteriv_func_t;
typedef __typeof__(&glGetTexParameterfv) renderer_gl_get_tex_parameterfv_func_t;
typedef __typeof__(&glGetTexParameteriv) renderer_gl_get_tex_parameteriv_func_t;
typedef __typeof__(&glIndexMask) renderer_gl_index_mask_func_t;
typedef __typeof__(&glIndexPointer) renderer_gl_index_pointer_func_t;
typedef __typeof__(&glIndexd) renderer_gl_indexd_func_t;
typedef __typeof__(&glIndexdv) renderer_gl_indexdv_func_t;
typedef __typeof__(&glIndexf) renderer_gl_indexf_func_t;
typedef __typeof__(&glIndexfv) renderer_gl_indexfv_func_t;
typedef __typeof__(&glIndexi) renderer_gl_indexi_func_t;
typedef __typeof__(&glIndexiv) renderer_gl_indexiv_func_t;
typedef __typeof__(&glIndexs) renderer_gl_indexs_func_t;
typedef __typeof__(&glIndexsv) renderer_gl_indexsv_func_t;
typedef __typeof__(&glIndexub) renderer_gl_indexub_func_t;
typedef __typeof__(&glIndexubv) renderer_gl_indexubv_func_t;
typedef __typeof__(&glInterleavedArrays) renderer_gl_interleaved_arrays_func_t;
typedef __typeof__(&glIsEnabled) renderer_gl_is_enabled_func_t;
typedef __typeof__(&glIsList) renderer_gl_is_list_func_t;
typedef __typeof__(&glIsTexture) renderer_gl_is_texture_func_t;
typedef __typeof__(&glLightModelf) renderer_gl_light_modelf_func_t;
typedef __typeof__(&glLightModelfv) renderer_gl_light_modelfv_func_t;
typedef __typeof__(&glLightModeli) renderer_gl_light_modeli_func_t;
typedef __typeof__(&glLightModeliv) renderer_gl_light_modeliv_func_t;
typedef __typeof__(&glLightf) renderer_gl_lightf_func_t;
typedef __typeof__(&glLightfv) renderer_gl_lightfv_func_t;
typedef __typeof__(&glLighti) renderer_gl_lighti_func_t;
typedef __typeof__(&glLightiv) renderer_gl_lightiv_func_t;
typedef __typeof__(&glLineStipple) renderer_gl_line_stipple_func_t;
typedef __typeof__(&glLineWidth) renderer_gl_line_width_func_t;
typedef __typeof__(&glListBase) renderer_gl_list_base_func_t;
typedef __typeof__(&glLoadMatrixd) renderer_gl_load_matrixd_func_t;
typedef __typeof__(&glLoadMatrixf) renderer_gl_load_matrixf_func_t;
typedef __typeof__(&glLoadName) renderer_gl_load_name_func_t;
typedef __typeof__(&glLogicOp) renderer_gl_logic_op_func_t;
typedef __typeof__(&glMap1d) renderer_gl_map1d_func_t;
typedef __typeof__(&glMap1f) renderer_gl_map1f_func_t;
typedef __typeof__(&glMap2d) renderer_gl_map2d_func_t;
typedef __typeof__(&glMap2f) renderer_gl_map2f_func_t;
typedef __typeof__(&glMapGrid1d) renderer_gl_map_grid1d_func_t;
typedef __typeof__(&glMapGrid1f) renderer_gl_map_grid1f_func_t;
typedef __typeof__(&glMapGrid2d) renderer_gl_map_grid2d_func_t;
typedef __typeof__(&glMapGrid2f) renderer_gl_map_grid2f_func_t;
typedef __typeof__(&glMaterialf) renderer_gl_materialf_func_t;
typedef __typeof__(&glMaterialfv) renderer_gl_materialfv_func_t;
typedef __typeof__(&glMateriali) renderer_gl_materiali_func_t;
typedef __typeof__(&glMaterialiv) renderer_gl_materialiv_func_t;
typedef __typeof__(&glMatrixMode) renderer_gl_matrix_mode_func_t;
typedef __typeof__(&glMultMatrixd) renderer_gl_mult_matrixd_func_t;
typedef __typeof__(&glMultMatrixf) renderer_gl_mult_matrixf_func_t;
typedef __typeof__(&glNewList) renderer_gl_new_list_func_t;
typedef __typeof__(&glNormal3b) renderer_gl_normal3b_func_t;
typedef __typeof__(&glNormal3bv) renderer_gl_normal3bv_func_t;
typedef __typeof__(&glNormal3d) renderer_gl_normal3d_func_t;
typedef __typeof__(&glNormal3dv) renderer_gl_normal3dv_func_t;
typedef __typeof__(&glNormal3f) renderer_gl_normal3f_func_t;
typedef __typeof__(&glNormal3fv) renderer_gl_normal3fv_func_t;
typedef __typeof__(&glNormal3i) renderer_gl_normal3i_func_t;
typedef __typeof__(&glNormal3iv) renderer_gl_normal3iv_func_t;
typedef __typeof__(&glNormal3s) renderer_gl_normal3s_func_t;
typedef __typeof__(&glNormal3sv) renderer_gl_normal3sv_func_t;
typedef __typeof__(&glOrtho) renderer_gl_ortho_func_t;
typedef __typeof__(&glPassThrough) renderer_gl_pass_through_func_t;
typedef __typeof__(&glPixelMapfv) renderer_gl_pixel_mapfv_func_t;
typedef __typeof__(&glPixelMapuiv) renderer_gl_pixel_mapuiv_func_t;
typedef __typeof__(&glPixelMapusv) renderer_gl_pixel_mapusv_func_t;
typedef __typeof__(&glPixelStoref) renderer_gl_pixel_storef_func_t;
typedef __typeof__(&glPixelStorei) renderer_gl_pixel_storei_func_t;
typedef __typeof__(&glPixelTransferf) renderer_gl_pixel_transferf_func_t;
typedef __typeof__(&glPixelTransferi) renderer_gl_pixel_transferi_func_t;
typedef __typeof__(&glPixelZoom) renderer_gl_pixel_zoom_func_t;
typedef __typeof__(&glPointSize) renderer_gl_point_size_func_t;
typedef __typeof__(&glPolygonOffset) renderer_gl_polygon_offset_func_t;
typedef __typeof__(&glPolygonStipple) renderer_gl_polygon_stipple_func_t;
typedef __typeof__(&glPrioritizeTextures) renderer_gl_prioritize_textures_func_t;
typedef __typeof__(&glPushAttrib) renderer_gl_attrib_mask_func_t;
typedef __typeof__(&glPushName) renderer_gl_push_name_func_t;
typedef __typeof__(&glRasterPos2d) renderer_gl_raster_pos2d_func_t;
typedef __typeof__(&glRasterPos2dv) renderer_gl_raster_pos2dv_func_t;
typedef __typeof__(&glRasterPos2f) renderer_gl_raster_pos2f_func_t;
typedef __typeof__(&glRasterPos2fv) renderer_gl_raster_pos2fv_func_t;
typedef __typeof__(&glRasterPos2i) renderer_gl_raster_pos2i_func_t;
typedef __typeof__(&glRasterPos2iv) renderer_gl_raster_pos2iv_func_t;
typedef __typeof__(&glRasterPos2s) renderer_gl_raster_pos2s_func_t;
typedef __typeof__(&glRasterPos2sv) renderer_gl_raster_pos2sv_func_t;
typedef __typeof__(&glRasterPos3d) renderer_gl_raster_pos3d_func_t;
typedef __typeof__(&glRasterPos3dv) renderer_gl_raster_pos3dv_func_t;
typedef __typeof__(&glRasterPos3f) renderer_gl_raster_pos3f_func_t;
typedef __typeof__(&glRasterPos3fv) renderer_gl_raster_pos3fv_func_t;
typedef __typeof__(&glRasterPos3i) renderer_gl_raster_pos3i_func_t;
typedef __typeof__(&glRasterPos3iv) renderer_gl_raster_pos3iv_func_t;
typedef __typeof__(&glRasterPos3s) renderer_gl_raster_pos3s_func_t;
typedef __typeof__(&glRasterPos3sv) renderer_gl_raster_pos3sv_func_t;
typedef __typeof__(&glRasterPos4d) renderer_gl_raster_pos4d_func_t;
typedef __typeof__(&glRasterPos4dv) renderer_gl_raster_pos4dv_func_t;
typedef __typeof__(&glRasterPos4f) renderer_gl_raster_pos4f_func_t;
typedef __typeof__(&glRasterPos4fv) renderer_gl_raster_pos4fv_func_t;
typedef __typeof__(&glRasterPos4i) renderer_gl_raster_pos4i_func_t;
typedef __typeof__(&glRasterPos4iv) renderer_gl_raster_pos4iv_func_t;
typedef __typeof__(&glRasterPos4s) renderer_gl_raster_pos4s_func_t;
typedef __typeof__(&glRasterPos4sv) renderer_gl_raster_pos4sv_func_t;
typedef __typeof__(&glReadBuffer) renderer_gl_read_buffer_func_t;
typedef __typeof__(&glReadPixels) renderer_gl_read_pixels_func_t;
typedef __typeof__(&glRectd) renderer_gl_rectd_func_t;
typedef __typeof__(&glRectdv) renderer_gl_rectdv_func_t;
typedef __typeof__(&glRectf) renderer_gl_rectf_func_t;
typedef __typeof__(&glRectfv) renderer_gl_rectfv_func_t;
typedef __typeof__(&glRecti) renderer_gl_recti_func_t;
typedef __typeof__(&glRectiv) renderer_gl_rectiv_func_t;
typedef __typeof__(&glRects) renderer_gl_rects_func_t;
typedef __typeof__(&glRectsv) renderer_gl_rectsv_func_t;
typedef __typeof__(&glRenderMode) renderer_gl_render_mode_func_t;
typedef __typeof__(&glRotated) renderer_gl_rotated_func_t;
typedef __typeof__(&glRotatef) renderer_gl_rotatef_func_t;
typedef __typeof__(&glScaled) renderer_gl_scaled_func_t;
typedef __typeof__(&glScalef) renderer_gl_scalef_func_t;
typedef __typeof__(&glSelectBuffer) renderer_gl_select_buffer_func_t;
typedef __typeof__(&glShadeModel) renderer_gl_shade_model_func_t;
typedef __typeof__(&glStencilFunc) renderer_gl_stencil_func_t;
typedef __typeof__(&glStencilMask) renderer_gl_stencil_mask_func_t;
typedef __typeof__(&glStencilOp) renderer_gl_stencil_op_func_t;
typedef __typeof__(&glTexCoord1d) renderer_gl_tex_coord1d_func_t;
typedef __typeof__(&glTexCoord1dv) renderer_gl_tex_coord1dv_func_t;
typedef __typeof__(&glTexCoord1f) renderer_gl_tex_coord1f_func_t;
typedef __typeof__(&glTexCoord1fv) renderer_gl_tex_coord1fv_func_t;
typedef __typeof__(&glTexCoord1i) renderer_gl_tex_coord1i_func_t;
typedef __typeof__(&glTexCoord1iv) renderer_gl_tex_coord1iv_func_t;
typedef __typeof__(&glTexCoord1s) renderer_gl_tex_coord1s_func_t;
typedef __typeof__(&glTexCoord1sv) renderer_gl_tex_coord1sv_func_t;
typedef __typeof__(&glTexCoord2d) renderer_gl_tex_coord2d_func_t;
typedef __typeof__(&glTexCoord2dv) renderer_gl_tex_coord2dv_func_t;
typedef __typeof__(&glTexCoord2f) renderer_gl_tex_coord2f_func_t;
typedef __typeof__(&glTexCoord2fv) renderer_gl_tex_coord2fv_func_t;
typedef __typeof__(&glTexCoord2i) renderer_gl_tex_coord2i_func_t;
typedef __typeof__(&glTexCoord2iv) renderer_gl_tex_coord2iv_func_t;
typedef __typeof__(&glTexCoord2s) renderer_gl_tex_coord2s_func_t;
typedef __typeof__(&glTexCoord2sv) renderer_gl_tex_coord2sv_func_t;
typedef __typeof__(&glTexCoord3d) renderer_gl_tex_coord3d_func_t;
typedef __typeof__(&glTexCoord3dv) renderer_gl_tex_coord3dv_func_t;
typedef __typeof__(&glTexCoord3f) renderer_gl_tex_coord3f_func_t;
typedef __typeof__(&glTexCoord3fv) renderer_gl_tex_coord3fv_func_t;
typedef __typeof__(&glTexCoord3i) renderer_gl_tex_coord3i_func_t;
typedef __typeof__(&glTexCoord3iv) renderer_gl_tex_coord3iv_func_t;
typedef __typeof__(&glTexCoord3s) renderer_gl_tex_coord3s_func_t;
typedef __typeof__(&glTexCoord3sv) renderer_gl_tex_coord3sv_func_t;
typedef __typeof__(&glTexCoord4d) renderer_gl_tex_coord4d_func_t;
typedef __typeof__(&glTexCoord4dv) renderer_gl_tex_coord4dv_func_t;
typedef __typeof__(&glTexCoord4f) renderer_gl_tex_coord4f_func_t;
typedef __typeof__(&glTexCoord4fv) renderer_gl_tex_coord4fv_func_t;
typedef __typeof__(&glTexCoord4i) renderer_gl_tex_coord4i_func_t;
typedef __typeof__(&glTexCoord4iv) renderer_gl_tex_coord4iv_func_t;
typedef __typeof__(&glTexCoord4s) renderer_gl_tex_coord4s_func_t;
typedef __typeof__(&glTexCoord4sv) renderer_gl_tex_coord4sv_func_t;
typedef __typeof__(&glTexEnvfv) renderer_gl_tex_envfv_func_t;
typedef __typeof__(&glTexEnviv) renderer_gl_tex_enviv_func_t;
typedef __typeof__(&glTexGend) renderer_gl_tex_gend_func_t;
typedef __typeof__(&glTexGendv) renderer_gl_tex_gendv_func_t;
typedef __typeof__(&glTexGenf) renderer_gl_tex_genf_func_t;
typedef __typeof__(&glTexGenfv) renderer_gl_tex_genfv_func_t;
typedef __typeof__(&glTexGeni) renderer_gl_tex_geni_func_t;
typedef __typeof__(&glTexGeniv) renderer_gl_tex_geniv_func_t;
typedef __typeof__(&glTexImage1D) renderer_gl_tex_image_1d_func_t;
typedef __typeof__(&glTexImage2D) renderer_gl_tex_image_2d_func_t;
typedef __typeof__(&glTexParameterfv) renderer_gl_tex_parameterfv_func_t;
typedef __typeof__(&glTexParameteriv) renderer_gl_tex_parameteriv_func_t;
typedef __typeof__(&glTexSubImage1D) renderer_gl_tex_sub_image_1d_func_t;
typedef __typeof__(&glTexSubImage2D) renderer_gl_tex_sub_image_2d_func_t;
typedef __typeof__(&glTranslated) renderer_gl_translated_func_t;
typedef __typeof__(&glTranslatef) renderer_gl_translatef_func_t;
typedef __typeof__(&glVertex2d) renderer_gl_vertex2d_func_t;
typedef __typeof__(&glVertex2dv) renderer_gl_vertex2dv_func_t;
typedef __typeof__(&glVertex2f) renderer_gl_vertex2f_func_t;
typedef __typeof__(&glVertex2fv) renderer_gl_vertex2fv_func_t;
typedef __typeof__(&glVertex2i) renderer_gl_vertex2i_func_t;
typedef __typeof__(&glVertex2iv) renderer_gl_vertex2iv_func_t;
typedef __typeof__(&glVertex2s) renderer_gl_vertex2s_func_t;
typedef __typeof__(&glVertex2sv) renderer_gl_vertex2sv_func_t;
typedef __typeof__(&glVertex3d) renderer_gl_vertex3d_func_t;
typedef __typeof__(&glVertex3dv) renderer_gl_vertex3dv_func_t;
typedef __typeof__(&glVertex3f) renderer_gl_vertex3f_func_t;
typedef __typeof__(&glVertex3fv) renderer_gl_vertex3fv_func_t;
typedef __typeof__(&glVertex3i) renderer_gl_vertex3i_func_t;
typedef __typeof__(&glVertex3iv) renderer_gl_vertex3iv_func_t;
typedef __typeof__(&glVertex3s) renderer_gl_vertex3s_func_t;
typedef __typeof__(&glVertex3sv) renderer_gl_vertex3sv_func_t;
typedef __typeof__(&glVertex4d) renderer_gl_vertex4d_func_t;
typedef __typeof__(&glVertex4dv) renderer_gl_vertex4dv_func_t;
typedef __typeof__(&glVertex4f) renderer_gl_vertex4f_func_t;
typedef __typeof__(&glVertex4fv) renderer_gl_vertex4fv_func_t;
typedef __typeof__(&glVertex4i) renderer_gl_vertex4i_func_t;
typedef __typeof__(&glVertex4iv) renderer_gl_vertex4iv_func_t;
typedef __typeof__(&glVertex4s) renderer_gl_vertex4s_func_t;
typedef __typeof__(&glVertex4sv) renderer_gl_vertex4sv_func_t;

#endif
