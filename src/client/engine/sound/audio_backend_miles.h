#ifndef CODUOMP_AUDIO_BACKEND_MILES_H
#define CODUOMP_AUDIO_BACKEND_MILES_H

#include "sound_system.h"

#if defined(_WIN32) && UINTPTR_MAX == UINT32_MAX
#define CODUOMP_HAS_MILES

/* The legacy i686 Windows build resolves this ABI from mss32.dll. Keep the
 * DLL's stdcall signatures separate from the native backend dispatch ABI. */
typedef struct coduomp_miles_api_s {
    int32_t (AUDIO_CALLBACK *api_set_preference)(int32_t preference, int32_t value);
    void (AUDIO_CALLBACK *api_set_file_callbacks)(audio_file_open_callback_t openCallback, audio_file_close_callback_t closeCallback, audio_file_seek_callback_t seekCallback, audio_file_read_callback_t readCallback);
    char * (AUDIO_CALLBACK *api_set_redist_directory)(const char *directory);
    int32_t (AUDIO_CALLBACK *api_startup)(void);
    void (AUDIO_CALLBACK *api_shutdown)(void);
    void (AUDIO_CALLBACK *api_close_3D_provider)(audio_provider_t provider);
    int32_t (AUDIO_CALLBACK *api_set_DirectSound_HWND)(audio_driver_t driver, audio_window_handle_t windowHandle);
    int32_t (AUDIO_CALLBACK *api_WAV_info)(const void *fileData, audio_sound_info_t *soundInfo);
    uint32_t (AUDIO_CALLBACK *api_size_processed_digital_audio)(uint32_t sampleRate, audio_sample_type_t sampleType, int32_t bufferCount, const audio_sound_info_t *sourceInfo);
    int32_t (AUDIO_CALLBACK *api_process_digital_audio)(void *destination, uint32_t destinationSize, uint32_t sampleRate, audio_sample_type_t sampleType, int32_t bufferCount, audio_sound_info_t *sourceInfo);
    int32_t (AUDIO_CALLBACK *api_digital_CPU_percent)(audio_driver_t driver);
    audio_driver_t (AUDIO_CALLBACK *api_open_digital_driver)(int32_t sampleRate, int32_t sampleFormat, int32_t channels, int32_t flags);
    const char * (AUDIO_CALLBACK *api_last_error)(void);
    int32_t (AUDIO_CALLBACK *api_enumerate_3D_providers)(audio_provider_enumerator_t *enumerator, audio_provider_t *provider, const char **providerName);
    int32_t (AUDIO_CALLBACK *api_open_3D_provider)(audio_provider_t provider);
    int32_t (AUDIO_CALLBACK *api_3D_provider_attribute)(audio_provider_t provider, const char *attributeName, void *value);
    int32_t (AUDIO_CALLBACK *api_set_3D_provider_preference)(audio_provider_t provider, const char *preferenceName, void *value);
    void (AUDIO_CALLBACK *api_set_3D_distance_factor)(audio_provider_t provider, float distanceFactor);
    audio_sample_handle_t (AUDIO_CALLBACK *api_allocate_sample_handle)(audio_driver_t driver);
    audio_3d_sample_handle_t (AUDIO_CALLBACK *api_allocate_3D_sample_handle)(audio_provider_t provider);
    void (AUDIO_CALLBACK *api_set_3D_position)(audio_3d_sample_handle_t sample, float x, float y, float z);
    void (AUDIO_CALLBACK *api_end_sample)(audio_sample_handle_t sample);
    void (AUDIO_CALLBACK *api_stop_sample)(audio_sample_handle_t sample);
    void (AUDIO_CALLBACK *api_resume_sample)(audio_sample_handle_t sample);
    int32_t (AUDIO_CALLBACK *api_sample_status)(audio_sample_handle_t sample);
    void (AUDIO_CALLBACK *api_end_3D_sample)(audio_3d_sample_handle_t sample);
    void (AUDIO_CALLBACK *api_stop_3D_sample)(audio_3d_sample_handle_t sample);
    void (AUDIO_CALLBACK *api_resume_3D_sample)(audio_3d_sample_handle_t sample);
    int32_t (AUDIO_CALLBACK *api_3D_sample_status)(audio_3d_sample_handle_t sample);
    audio_stream_handle_t (AUDIO_CALLBACK *api_open_stream)(audio_driver_t driver, const char *filename, int32_t streamMemory);
    void (AUDIO_CALLBACK *api_close_stream)(audio_stream_handle_t stream);
    void (AUDIO_CALLBACK *api_pause_stream)(audio_stream_handle_t stream, qboolean paused);
    int32_t (AUDIO_CALLBACK *api_stream_status)(audio_stream_handle_t stream);
    int32_t (AUDIO_CALLBACK *api_stream_playback_rate)(audio_stream_handle_t stream);
    void (AUDIO_CALLBACK *api_set_stream_playback_rate)(audio_stream_handle_t stream, int32_t playbackRate);
    void (AUDIO_CALLBACK *api_set_stream_volume_pan)(audio_stream_handle_t stream, float volume, float pan);
    void (AUDIO_CALLBACK *api_stream_volume_pan)(audio_stream_handle_t stream, float *volume, float *pan);
    void (AUDIO_CALLBACK *api_set_stream_loop_count)(audio_stream_handle_t stream, int32_t loopCount);
    void (AUDIO_CALLBACK *api_set_stream_reverb_levels)(audio_stream_handle_t stream, float dryLevel, float wetLevel);
    void (AUDIO_CALLBACK *api_stream_ms_position)(audio_stream_handle_t stream, int32_t *totalMsec, int32_t *currentMsec);
    void (AUDIO_CALLBACK *api_set_stream_ms_position)(audio_stream_handle_t stream, int32_t positionMsec);
    void (AUDIO_CALLBACK *api_start_stream)(audio_stream_handle_t stream);
    void (AUDIO_CALLBACK *api_init_sample)(audio_sample_handle_t sample);
    void (AUDIO_CALLBACK *api_set_sample_type)(audio_sample_handle_t sample, audio_sample_type_t sampleType, int32_t flags);
    void (AUDIO_CALLBACK *api_set_sample_address)(audio_sample_handle_t sample, const void *data, uint32_t dataLength);
    void (AUDIO_CALLBACK *api_set_sample_adpcm_block_size)(audio_sample_handle_t sample, uint32_t blockSize);
    int32_t (AUDIO_CALLBACK *api_sample_playback_rate)(audio_sample_handle_t sample);
    void (AUDIO_CALLBACK *api_set_sample_playback_rate)(audio_sample_handle_t sample, int32_t playbackRate);
    void (AUDIO_CALLBACK *api_set_sample_volume_pan)(audio_sample_handle_t sample, float volume, float pan);
    void (AUDIO_CALLBACK *api_sample_volume_pan)(audio_sample_handle_t sample, float *volume, float *pan);
    void (AUDIO_CALLBACK *api_set_sample_loop_count)(audio_sample_handle_t sample, int32_t loopCount);
    void (AUDIO_CALLBACK *api_set_sample_reverb_levels)(audio_sample_handle_t sample, float dryLevel, float wetLevel);
    void (AUDIO_CALLBACK *api_sample_ms_position)(audio_sample_handle_t sample, int32_t *totalMsec, int32_t *currentMsec);
    void (AUDIO_CALLBACK *api_set_sample_ms_position)(audio_sample_handle_t sample, int32_t positionMsec);
    void (AUDIO_CALLBACK *api_start_sample)(audio_sample_handle_t sample);
    void (AUDIO_CALLBACK *api_release_sample_handle)(audio_sample_handle_t sample);
    int32_t (AUDIO_CALLBACK *api_minimum_sample_buffer_size)(audio_driver_t driver, int32_t sampleRate, audio_sample_type_t sampleType);
    uint32_t (AUDIO_CALLBACK *api_sample_position)(audio_sample_handle_t sample);
    int32_t (AUDIO_CALLBACK *api_sample_buffer_ready)(audio_sample_handle_t sample);
    void (AUDIO_CALLBACK *api_load_sample_buffer)(audio_sample_handle_t sample, int32_t bufferIndex, const void *data, int32_t byteCount);
    int32_t (AUDIO_CALLBACK *api_set_3D_sample_info)(audio_3d_sample_handle_t sample, const snd_alias_sound_file_t *soundFile);
    void (AUDIO_CALLBACK *api_set_3D_sample_volume)(audio_3d_sample_handle_t sample, float volume);
    void (AUDIO_CALLBACK *api_set_3D_sample_distances)(audio_3d_sample_handle_t sample, float maximumDistance, float minimumDistance);
    int32_t (AUDIO_CALLBACK *api_3D_sample_playback_rate)(audio_3d_sample_handle_t sample);
    uint32_t (AUDIO_CALLBACK *api_3D_sample_offset)(audio_3d_sample_handle_t sample);
    uint32_t (AUDIO_CALLBACK *api_3D_sample_length)(audio_3d_sample_handle_t sample);
    float (AUDIO_CALLBACK *api_3D_sample_volume)(audio_3d_sample_handle_t sample);
    void (AUDIO_CALLBACK *api_3D_position)(audio_3d_sample_handle_t sample, float *x, float *y, float *z);
    void (AUDIO_CALLBACK *api_set_3D_sample_playback_rate)(audio_3d_sample_handle_t sample, int32_t playbackRate);
    void (AUDIO_CALLBACK *api_set_3D_sample_loop_count)(audio_3d_sample_handle_t sample, int32_t loopCount);
    void (AUDIO_CALLBACK *api_set_3D_sample_effects_level)(audio_3d_sample_handle_t sample, float effectsLevel);
    int32_t (AUDIO_CALLBACK *api_set_3D_sample_preference)(audio_3d_sample_handle_t sample, const char *preferenceName, void *value);
    void (AUDIO_CALLBACK *api_set_digital_master_room_type)(audio_driver_t driver, int32_t roomType);
    void (AUDIO_CALLBACK *api_set_3D_room_type)(audio_provider_t provider, int32_t roomType);
    void (AUDIO_CALLBACK *api_set_3D_sample_offset)(audio_3d_sample_handle_t sample, int32_t byteOffset);
    void (AUDIO_CALLBACK *api_start_3D_sample)(audio_3d_sample_handle_t sample);
} coduomp_miles_api_t;

/* NOT_FROM_ORIGINAL_SOURCE: load optional Miles exports as one complete API.
 * A successful load stays resident for the process lifetime, like retail's
 * import table, so shutdown and atexit callbacks never reference unloaded code. */
const coduomp_miles_api_t *coduomp_miles_load(void);
#endif

#endif
