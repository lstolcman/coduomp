#ifndef AUDIO_BACKEND_API_H
#define AUDIO_BACKEND_API_H

#include "sound_system.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct audio_backend_api_s {
    const char *name;
    void (*api_bind_loaded_sample)(audio_sample_handle_t sample,
                                   const snd_alias_sound_file_t *soundFile);
    void (*api_forget_loaded_sound)(
        const snd_alias_sound_file_t *soundFile);
    int32_t (*api_set_preference)(int32_t preference, int32_t value);
    void (*api_set_file_callbacks)(
        audio_file_open_callback_t openCallback,
        audio_file_close_callback_t closeCallback,
        audio_file_seek_callback_t seekCallback,
        audio_file_read_callback_t readCallback);
    void (*api_set_redist_directory)(const char *directory);
    int32_t (*api_startup)(void);
    void (*api_shutdown)(void);
    void (*api_close_3D_provider)(audio_provider_t provider);
    void (*api_set_DirectSound_HWND)(audio_driver_t driver, audio_window_handle_t windowHandle);
    int32_t (*api_WAV_info)(const void *fileData, audio_sound_info_t *soundInfo);
    uint32_t (*api_size_processed_digital_audio)(
        uint32_t sampleRate, audio_sample_type_t sampleType,
        int32_t bufferCount, const audio_sound_info_t *sourceInfo);
    int32_t (*api_process_digital_audio)(
        void *destination, uint32_t destinationSize, uint32_t sampleRate,
        audio_sample_type_t sampleType, int32_t bufferCount,
        audio_sound_info_t *sourceInfo);
    int32_t (*api_digital_CPU_percent)(audio_driver_t driver);
    audio_driver_t (*api_open_digital_driver)(int32_t sampleRate, int32_t sampleFormat, int32_t channels, int32_t flags);
    const char * (*api_last_error)(void);
    int32_t (*api_enumerate_3D_providers)(audio_provider_enumerator_t *enumerator, audio_provider_t *provider, const char **providerName);
    int32_t (*api_open_3D_provider)(audio_provider_t provider);
    int32_t (*api_3D_provider_attribute)(audio_provider_t provider, const char *attributeName, void *value);
    int32_t (*api_set_3D_provider_preference)(audio_provider_t provider, const char *preferenceName, void *value);
    void (*api_set_3D_distance_factor)(audio_provider_t provider, float distanceFactor);
    audio_sample_handle_t (*api_allocate_sample_handle)(audio_driver_t driver);
    audio_3d_sample_handle_t (*api_allocate_3D_sample_handle)(audio_provider_t provider);
    void (*api_set_3D_position)(audio_3d_sample_handle_t sample, float x, float y, float z);
    void (*api_end_sample)(audio_sample_handle_t sample);
    void (*api_stop_sample)(audio_sample_handle_t sample);
    void (*api_resume_sample)(audio_sample_handle_t sample);
    int32_t (*api_sample_status)(audio_sample_handle_t sample);
    void (*api_end_3D_sample)(audio_3d_sample_handle_t sample);
    void (*api_stop_3D_sample)(audio_3d_sample_handle_t sample);
    void (*api_resume_3D_sample)(audio_3d_sample_handle_t sample);
    int32_t (*api_3D_sample_status)(audio_3d_sample_handle_t sample);
    audio_stream_handle_t (*api_open_stream)(audio_driver_t driver, const char *filename, int32_t streamMemory);
    void (*api_close_stream)(audio_stream_handle_t stream);
    void (*api_pause_stream)(audio_stream_handle_t stream, qboolean paused);
    int32_t (*api_stream_status)(audio_stream_handle_t stream);
    int32_t (*api_stream_playback_rate)(audio_stream_handle_t stream);
    void (*api_set_stream_playback_rate)(audio_stream_handle_t stream, int32_t playbackRate);
    void (*api_set_stream_volume_pan)(audio_stream_handle_t stream, float volume, float pan);
    void (*api_stream_volume_pan)(audio_stream_handle_t stream, float *volume, float *pan);
    void (*api_set_stream_loop_count)(audio_stream_handle_t stream, int32_t loopCount);
    void (*api_set_stream_reverb_levels)(audio_stream_handle_t stream, float dryLevel, float wetLevel);
    void (*api_stream_ms_position)(audio_stream_handle_t stream, int32_t *totalMsec, int32_t *currentMsec);
    void (*api_set_stream_ms_position)(audio_stream_handle_t stream, int32_t positionMsec);
    void (*api_start_stream)(audio_stream_handle_t stream);
    void (*api_init_sample)(audio_sample_handle_t sample);
    void (*api_set_sample_type)(audio_sample_handle_t sample, audio_sample_type_t sampleType, int32_t flags);
    void (*api_set_sample_address)(audio_sample_handle_t sample, const void *data, uint32_t dataLength);
    void (*api_set_sample_adpcm_block_size)(audio_sample_handle_t sample, uint32_t blockSize);
    int32_t (*api_sample_playback_rate)(audio_sample_handle_t sample);
    void (*api_set_sample_playback_rate)(audio_sample_handle_t sample, int32_t playbackRate);
    void (*api_set_sample_volume_pan)(audio_sample_handle_t sample, float volume, float pan);
    void (*api_sample_volume_pan)(audio_sample_handle_t sample, float *volume, float *pan);
    void (*api_set_sample_loop_count)(audio_sample_handle_t sample, int32_t loopCount);
    void (*api_set_sample_reverb_levels)(audio_sample_handle_t sample, float dryLevel, float wetLevel);
    void (*api_sample_ms_position)(audio_sample_handle_t sample, int32_t *totalMsec, int32_t *currentMsec);
    void (*api_set_sample_ms_position)(audio_sample_handle_t sample, int32_t positionMsec);
    void (*api_start_sample)(audio_sample_handle_t sample);
    void (*api_release_sample_handle)(audio_sample_handle_t sample);
    int32_t (*api_minimum_sample_buffer_size)(audio_driver_t driver, int32_t sampleRate, audio_sample_type_t sampleType);
    uint32_t (*api_sample_position)(audio_sample_handle_t sample);
    int32_t (*api_sample_buffer_ready)(audio_sample_handle_t sample);
    void (*api_load_sample_buffer)(audio_sample_handle_t sample, int32_t bufferIndex, const void *data, int32_t byteCount);
    void (*api_set_3D_sample_info)(audio_3d_sample_handle_t sample, const snd_alias_sound_file_t *soundFile);
    void (*api_set_3D_sample_volume)(audio_3d_sample_handle_t sample, float volume);
    void (*api_set_3D_sample_distances)(audio_3d_sample_handle_t sample, float maximumDistance, float minimumDistance);
    int32_t (*api_3D_sample_playback_rate)(audio_3d_sample_handle_t sample);
    uint32_t (*api_3D_sample_offset)(audio_3d_sample_handle_t sample);
    uint32_t (*api_3D_sample_length)(audio_3d_sample_handle_t sample);
    float (*api_3D_sample_volume)(audio_3d_sample_handle_t sample);
    void (*api_3D_position)(audio_3d_sample_handle_t sample, float *x, float *y, float *z);
    void (*api_set_3D_sample_playback_rate)(audio_3d_sample_handle_t sample, int32_t playbackRate);
    void (*api_set_3D_sample_loop_count)(audio_3d_sample_handle_t sample, int32_t loopCount);
    void (*api_set_3D_sample_effects_level)(audio_3d_sample_handle_t sample, float effectsLevel);
    int32_t (*api_set_3D_sample_preference)(audio_3d_sample_handle_t sample, const char *preferenceName, void *value);
    void (*api_set_digital_master_room_type)(audio_driver_t driver, int32_t roomType);
    void (*api_set_3D_room_type)(audio_provider_t provider, int32_t roomType);
    void (*api_set_3D_sample_offset)(audio_3d_sample_handle_t sample, int32_t byteOffset);
    void (*api_start_3D_sample)(audio_3d_sample_handle_t sample);
} audio_backend_api_t;

/* NOT_FROM_ORIGINAL_SOURCE: select the complete native audio backend named by
 * the stock mss_3d_provider cvar, falling back to Miniaudio when unavailable. */
qboolean audio_select_backend(const char *preferredName, const char **selectedName);

/* NOT_FROM_ORIGINAL_SOURCE: query the active adapter without changing it. */
qboolean coduomp_audio_uses_miles(void);
const char *coduomp_audio_fallback_provider(void);
void audio_bind_loaded_sample(
    audio_sample_handle_t sample,
    const snd_alias_sound_file_t *soundFile);
void audio_forget_loaded_sound(
    const snd_alias_sound_file_t *soundFile);

int32_t audio_set_preference(int32_t preference, int32_t value);
void audio_set_file_callbacks(
    audio_file_open_callback_t openCallback,
    audio_file_close_callback_t closeCallback,
    audio_file_seek_callback_t seekCallback,
    audio_file_read_callback_t readCallback);
void audio_set_redist_directory(const char *directory);
int32_t audio_startup(void);
void audio_shutdown(void);
void audio_close_3D_provider(audio_provider_t provider);
void audio_set_DirectSound_HWND(audio_driver_t driver, audio_window_handle_t windowHandle);
int32_t audio_WAV_info(const void *fileData, audio_sound_info_t *soundInfo);
uint32_t audio_size_processed_digital_audio(
    uint32_t sampleRate, audio_sample_type_t sampleType,
    int32_t bufferCount, const audio_sound_info_t *sourceInfo);
int32_t audio_process_digital_audio(
    void *destination, uint32_t destinationSize, uint32_t sampleRate,
    audio_sample_type_t sampleType, int32_t bufferCount,
    audio_sound_info_t *sourceInfo);
int32_t audio_digital_CPU_percent(audio_driver_t driver);
audio_driver_t audio_open_digital_driver(int32_t sampleRate, int32_t sampleFormat, int32_t channels, int32_t flags);
const char * audio_last_error(void);
int32_t audio_enumerate_3D_providers(audio_provider_enumerator_t *enumerator, audio_provider_t *provider, const char **providerName);
int32_t audio_open_3D_provider(audio_provider_t provider);
int32_t audio_3D_provider_attribute(audio_provider_t provider, const char *attributeName, void *value);
int32_t audio_set_3D_provider_preference(audio_provider_t provider, const char *preferenceName, void *value);
void audio_set_3D_distance_factor(audio_provider_t provider, float distanceFactor);
audio_sample_handle_t audio_allocate_sample_handle(audio_driver_t driver);
audio_3d_sample_handle_t audio_allocate_3D_sample_handle(audio_provider_t provider);
void audio_set_3D_position(audio_3d_sample_handle_t sample, float x, float y, float z);
void audio_end_sample(audio_sample_handle_t sample);
void audio_stop_sample(audio_sample_handle_t sample);
void audio_resume_sample(audio_sample_handle_t sample);
int32_t audio_sample_status(audio_sample_handle_t sample);
void audio_end_3D_sample(audio_3d_sample_handle_t sample);
void audio_stop_3D_sample(audio_3d_sample_handle_t sample);
void audio_resume_3D_sample(audio_3d_sample_handle_t sample);
int32_t audio_3D_sample_status(audio_3d_sample_handle_t sample);
audio_stream_handle_t audio_open_stream(audio_driver_t driver, const char *filename, int32_t streamMemory);
void audio_close_stream(audio_stream_handle_t stream);
void audio_pause_stream(audio_stream_handle_t stream, qboolean paused);
int32_t audio_stream_status(audio_stream_handle_t stream);
int32_t audio_stream_playback_rate(audio_stream_handle_t stream);
void audio_set_stream_playback_rate(audio_stream_handle_t stream, int32_t playbackRate);
void audio_set_stream_volume_pan(audio_stream_handle_t stream, float volume, float pan);
void audio_stream_volume_pan(audio_stream_handle_t stream, float *volume, float *pan);
void audio_set_stream_loop_count(audio_stream_handle_t stream, int32_t loopCount);
void audio_set_stream_reverb_levels(audio_stream_handle_t stream, float dryLevel, float wetLevel);
void audio_stream_ms_position(audio_stream_handle_t stream, int32_t *totalMsec, int32_t *currentMsec);
void audio_set_stream_ms_position(audio_stream_handle_t stream, int32_t positionMsec);
void audio_start_stream(audio_stream_handle_t stream);
void audio_init_sample(audio_sample_handle_t sample);
void audio_set_sample_type(audio_sample_handle_t sample, audio_sample_type_t sampleType, int32_t flags);
void audio_set_sample_address(audio_sample_handle_t sample, const void *data, uint32_t dataLength);
void audio_set_sample_adpcm_block_size(audio_sample_handle_t sample, uint32_t blockSize);
int32_t audio_sample_playback_rate(audio_sample_handle_t sample);
void audio_set_sample_playback_rate(audio_sample_handle_t sample, int32_t playbackRate);
void audio_set_sample_volume_pan(audio_sample_handle_t sample, float volume, float pan);
void audio_sample_volume_pan(audio_sample_handle_t sample, float *volume, float *pan);
void audio_set_sample_loop_count(audio_sample_handle_t sample, int32_t loopCount);
void audio_set_sample_reverb_levels(audio_sample_handle_t sample, float dryLevel, float wetLevel);
void audio_sample_ms_position(audio_sample_handle_t sample, int32_t *totalMsec, int32_t *currentMsec);
void audio_set_sample_ms_position(audio_sample_handle_t sample, int32_t positionMsec);
void audio_start_sample(audio_sample_handle_t sample);
void audio_release_sample_handle(audio_sample_handle_t sample);
int32_t audio_minimum_sample_buffer_size(audio_driver_t driver, int32_t sampleRate, audio_sample_type_t sampleType);
uint32_t audio_sample_position(audio_sample_handle_t sample);
int32_t audio_sample_buffer_ready(audio_sample_handle_t sample);
void audio_load_sample_buffer(audio_sample_handle_t sample, int32_t bufferIndex, const void *data, int32_t byteCount);
void audio_set_3D_sample_info(audio_3d_sample_handle_t sample, const snd_alias_sound_file_t *soundFile);
void audio_set_3D_sample_volume(audio_3d_sample_handle_t sample, float volume);
void audio_set_3D_sample_distances(audio_3d_sample_handle_t sample, float maximumDistance, float minimumDistance);
int32_t audio_3D_sample_playback_rate(audio_3d_sample_handle_t sample);
uint32_t audio_3D_sample_offset(audio_3d_sample_handle_t sample);
uint32_t audio_3D_sample_length(audio_3d_sample_handle_t sample);
float audio_3D_sample_volume(audio_3d_sample_handle_t sample);
void audio_3D_position(audio_3d_sample_handle_t sample, float *x, float *y, float *z);
void audio_set_3D_sample_playback_rate(audio_3d_sample_handle_t sample, int32_t playbackRate);
void audio_set_3D_sample_loop_count(audio_3d_sample_handle_t sample, int32_t loopCount);
void audio_set_3D_sample_effects_level(audio_3d_sample_handle_t sample, float effectsLevel);
int32_t audio_set_3D_sample_preference(audio_3d_sample_handle_t sample, const char *preferenceName, void *value);
void audio_set_digital_master_room_type(audio_driver_t driver, int32_t roomType);
void audio_set_3D_room_type(audio_provider_t provider, int32_t roomType);
void audio_set_3D_sample_offset(audio_3d_sample_handle_t sample, int32_t byteOffset);
void audio_start_3D_sample(audio_3d_sample_handle_t sample);

#if defined(AUDIO_BACKEND_MINIAUDIO)
extern const audio_backend_api_t miniaudio_backend;
#endif
#if defined(AUDIO_BACKEND_OPENAL)
extern const audio_backend_api_t openal_backend;
#endif

#ifdef __cplusplus
}
#endif

#endif
