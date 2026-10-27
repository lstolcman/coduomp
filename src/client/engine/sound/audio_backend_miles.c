#include "audio_backend_miles.h"

#if defined(CODUOMP_HAS_MILES)
#include "client/engine/platform/dynamic_library_boundary.h"
#include <string.h>
#include <windows.h>

static void *miles_library;
static coduomp_miles_api_t miles_api;

/* NOT_FROM_ORIGINAL_SOURCE: replace mandatory PE imports with an all-or-nothing
 * runtime load beside the executable. Failed loads publish no pointers and
 * release the library. */
const coduomp_miles_api_t *coduomp_miles_load(void)
{
    if (miles_library != NULL)
        return &miles_api;

    static const char libraryName[] = "mss32.dll";
    char libraryPath[MAX_PATH];
    const DWORD pathLength = GetModuleFileNameA(NULL, libraryPath, sizeof(libraryPath));
    if (pathLength == 0 || pathLength >= sizeof(libraryPath))
        return NULL;
    char *filename = strrchr(libraryPath, '\\');
    if (filename == NULL)
        return NULL;
    ++filename;
    if ((size_t)(filename - libraryPath) + sizeof(libraryName) > sizeof(libraryPath))
        return NULL;
    memcpy(filename, libraryName, sizeof(libraryName));
    void *const library = coduomp_library_open(libraryPath);
    if (library == NULL)
        return NULL;

    coduomp_miles_api_t resolved = {0};
#define CODUOMP_MILES_RESOLVE(name, bytes) \
    do { \
        coduomp_library_symbol(library, "_AIL_" #name "@" #bytes, \
                               &resolved.api_##name, sizeof(resolved.api_##name)); \
        if (resolved.api_##name == NULL) \
            goto unavailable; \
    } while (0)
    CODUOMP_MILES_RESOLVE(set_preference, 8);
    CODUOMP_MILES_RESOLVE(set_file_callbacks, 16);
    CODUOMP_MILES_RESOLVE(set_redist_directory, 4);
    CODUOMP_MILES_RESOLVE(startup, 0);
    CODUOMP_MILES_RESOLVE(shutdown, 0);
    CODUOMP_MILES_RESOLVE(close_3D_provider, 4);
    CODUOMP_MILES_RESOLVE(set_DirectSound_HWND, 8);
    CODUOMP_MILES_RESOLVE(WAV_info, 8);
    CODUOMP_MILES_RESOLVE(size_processed_digital_audio, 16);
    CODUOMP_MILES_RESOLVE(process_digital_audio, 24);
    CODUOMP_MILES_RESOLVE(digital_CPU_percent, 4);
    CODUOMP_MILES_RESOLVE(open_digital_driver, 16);
    CODUOMP_MILES_RESOLVE(last_error, 0);
    CODUOMP_MILES_RESOLVE(enumerate_3D_providers, 12);
    CODUOMP_MILES_RESOLVE(open_3D_provider, 4);
    CODUOMP_MILES_RESOLVE(3D_provider_attribute, 12);
    CODUOMP_MILES_RESOLVE(set_3D_provider_preference, 12);
    CODUOMP_MILES_RESOLVE(set_3D_distance_factor, 8);
    CODUOMP_MILES_RESOLVE(allocate_sample_handle, 4);
    CODUOMP_MILES_RESOLVE(allocate_3D_sample_handle, 4);
    CODUOMP_MILES_RESOLVE(set_3D_position, 16);
    CODUOMP_MILES_RESOLVE(end_sample, 4);
    CODUOMP_MILES_RESOLVE(stop_sample, 4);
    CODUOMP_MILES_RESOLVE(resume_sample, 4);
    CODUOMP_MILES_RESOLVE(sample_status, 4);
    CODUOMP_MILES_RESOLVE(end_3D_sample, 4);
    CODUOMP_MILES_RESOLVE(stop_3D_sample, 4);
    CODUOMP_MILES_RESOLVE(resume_3D_sample, 4);
    CODUOMP_MILES_RESOLVE(3D_sample_status, 4);
    CODUOMP_MILES_RESOLVE(open_stream, 12);
    CODUOMP_MILES_RESOLVE(close_stream, 4);
    CODUOMP_MILES_RESOLVE(pause_stream, 8);
    CODUOMP_MILES_RESOLVE(stream_status, 4);
    CODUOMP_MILES_RESOLVE(stream_playback_rate, 4);
    CODUOMP_MILES_RESOLVE(set_stream_playback_rate, 8);
    CODUOMP_MILES_RESOLVE(set_stream_volume_pan, 12);
    CODUOMP_MILES_RESOLVE(stream_volume_pan, 12);
    CODUOMP_MILES_RESOLVE(set_stream_loop_count, 8);
    CODUOMP_MILES_RESOLVE(set_stream_reverb_levels, 12);
    CODUOMP_MILES_RESOLVE(stream_ms_position, 12);
    CODUOMP_MILES_RESOLVE(set_stream_ms_position, 8);
    CODUOMP_MILES_RESOLVE(start_stream, 4);
    CODUOMP_MILES_RESOLVE(init_sample, 4);
    CODUOMP_MILES_RESOLVE(set_sample_type, 12);
    CODUOMP_MILES_RESOLVE(set_sample_address, 12);
    CODUOMP_MILES_RESOLVE(set_sample_adpcm_block_size, 8);
    CODUOMP_MILES_RESOLVE(sample_playback_rate, 4);
    CODUOMP_MILES_RESOLVE(set_sample_playback_rate, 8);
    CODUOMP_MILES_RESOLVE(set_sample_volume_pan, 12);
    CODUOMP_MILES_RESOLVE(sample_volume_pan, 12);
    CODUOMP_MILES_RESOLVE(set_sample_loop_count, 8);
    CODUOMP_MILES_RESOLVE(set_sample_reverb_levels, 12);
    CODUOMP_MILES_RESOLVE(sample_ms_position, 12);
    CODUOMP_MILES_RESOLVE(set_sample_ms_position, 8);
    CODUOMP_MILES_RESOLVE(start_sample, 4);
    CODUOMP_MILES_RESOLVE(release_sample_handle, 4);
    CODUOMP_MILES_RESOLVE(minimum_sample_buffer_size, 12);
    CODUOMP_MILES_RESOLVE(sample_position, 4);
    CODUOMP_MILES_RESOLVE(sample_buffer_ready, 4);
    CODUOMP_MILES_RESOLVE(load_sample_buffer, 16);
    CODUOMP_MILES_RESOLVE(set_3D_sample_info, 8);
    CODUOMP_MILES_RESOLVE(set_3D_sample_volume, 8);
    CODUOMP_MILES_RESOLVE(set_3D_sample_distances, 12);
    CODUOMP_MILES_RESOLVE(3D_sample_playback_rate, 4);
    CODUOMP_MILES_RESOLVE(3D_sample_offset, 4);
    CODUOMP_MILES_RESOLVE(3D_sample_length, 4);
    CODUOMP_MILES_RESOLVE(3D_sample_volume, 4);
    CODUOMP_MILES_RESOLVE(3D_position, 16);
    CODUOMP_MILES_RESOLVE(set_3D_sample_playback_rate, 8);
    CODUOMP_MILES_RESOLVE(set_3D_sample_loop_count, 8);
    CODUOMP_MILES_RESOLVE(set_3D_sample_effects_level, 8);
    CODUOMP_MILES_RESOLVE(set_3D_sample_preference, 12);
    CODUOMP_MILES_RESOLVE(set_digital_master_room_type, 8);
    CODUOMP_MILES_RESOLVE(set_3D_room_type, 8);
    CODUOMP_MILES_RESOLVE(set_3D_sample_offset, 8);
    CODUOMP_MILES_RESOLVE(start_3D_sample, 4);
#undef CODUOMP_MILES_RESOLVE

    miles_api = resolved;
    miles_library = library;
    return &miles_api;

unavailable:
    (void)coduomp_library_close(library);
    return NULL;
}
#endif
