#include "q_shared.h"

#include "platform/crt_boundary.h"
#include "platform/sdl_platform.h"

#include <string.h>

enum {
    /* Highest video-memory tier used by United Offensive's shipped
     * configure_mp.csv recommendation table. Any measured value at or above
     * this tier selects the same highest-quality row, so it is also the
     * conservative value used when no native measurement is available. */
    CODUOMP_VIDEO_MEMORY_PROFILE_MB = 256
};

#if defined(_WIN32)
#define COBJMACROS
#include <windows.h>
#include <dxgi.h>
#endif

typedef struct videoMemoryRendererEstimate_s {
    const char *rendererPrefix;
    int32_t memoryMegabytes;
} videoMemoryRendererEstimate_t;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(_Alignof(videoMemoryRendererEstimate_t) == 4,
               "i386 renderer-memory estimate alignment changed");
_Static_assert(offsetof(videoMemoryRendererEstimate_t, rendererPrefix) == 0x00,
               "i386 renderer-memory prefix moved");
_Static_assert(offsetof(videoMemoryRendererEstimate_t, memoryMegabytes) == 0x04,
               "i386 renderer-memory estimate moved");
_Static_assert(sizeof(videoMemoryRendererEstimate_t) == 0x08,
               "i386 renderer-memory estimate stride changed");
#endif

#if defined(_WIN32)
/* NOT_FROM_ORIGINAL_SOURCE: the original Win32 probe measured local video
 * memory with DirectDraw. DXGI exposes the same dedicated-adapter quantity
 * without creating a window, device context, or OpenGL context. The entry
 * point is resolved dynamically so every supported toolchain can build the
 * probe without adding a link dependency. */
static uint32_t coduomp_windows_query_dedicated_video_memory(void)
{
    /* IID_IDXGIFactory, copied from the shipped SDK header. */
    static const GUID factoryId = {
        0x7b7166ec, 0x21c7, 0x44ae,
        {0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69}
    };
    typedef HRESULT (WINAPI *coduomp_create_dxgi_factory_fn)(
        REFIID interfaceId, void **factory);
    HMODULE dxgiLibrary;
    coduomp_create_dxgi_factory_fn createFactory;
    IDXGIFactory *factory = NULL;
    uint32_t maximumBytes = 0;

    dxgiLibrary = LoadLibraryA("dxgi.dll");
    if (dxgiLibrary == NULL)
        return 0;
    createFactory = (coduomp_create_dxgi_factory_fn)(uintptr_t)
        GetProcAddress(dxgiLibrary, "CreateDXGIFactory");
    if (createFactory == NULL ||
        FAILED(createFactory(&factoryId, (void **)&factory)) ||
        factory == NULL) {
        FreeLibrary(dxgiLibrary);
        return 0;
    }

    for (UINT index = 0;; ++index) {
        IDXGIAdapter *adapter = NULL;
        DXGI_ADAPTER_DESC description;

        if (factory->lpVtbl->EnumAdapters(factory, index, &adapter) != S_OK ||
            adapter == NULL) {
            break;
        }
        memset(&description, 0, sizeof(description));
        if (SUCCEEDED(adapter->lpVtbl->GetDesc(adapter, &description))) {
            SIZE_T bytes = description.DedicatedVideoMemory;
            if (bytes == 0)
                bytes = description.SharedSystemMemory;
            if (bytes > UINT32_MAX)
                bytes = UINT32_MAX;
            if ((uint32_t)bytes > maximumBytes)
                maximumBytes = (uint32_t)bytes;
        }
        adapter->lpVtbl->Release(adapter);
    }

    factory->lpVtbl->Release(factory);
    FreeLibrary(dxgiLibrary);
    return maximumBytes;
}
#endif

#if defined(_WIN32)
/* Source: CoDUOMP.exe 0x00469410..0x00469768.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00469410_00469769.mcode.
 * Role name: the OpenGL fallback passes GL_RENDERER here, and every table
 * result is shifted left by 20 before returning. Ordering is significant:
 * model-specific prefixes precede their generic family prefix. */
static uint32_t coduomp_estimate_video_memory_bytes_from_renderer(
    const char *renderer)
{
    /* The original constructs all 47 records in its 0x178-byte stack frame at
     * 0x00469427..0x00469714; there is no static estimate table. */
    const videoMemoryRendererEstimate_t estimates[] = {
        {"Radeon 7200", 64},
        {"Radeon 7500", 64},
        {"Radeon 8500", 64},
        {"Radeon 9000", 64},
        {"Radeon 9100", 64},
        {"Radeon 9200", 64},
        {"Radeon 9500", 64},
        {"Radeon 9600", 128},
        {"Radeon 9700", 128},
        {"Radeon 9800", 128},
        {"Radeon", 32},
        {"GeForce 256", 32},
        {"GeForce2 GTS", 32},
        {"GeForce2 MX", 32},
        {"GeForce2", 32},
        {"GeForce3", 64},
        {"GeForce4 420 Go", 64},
        {"GeForce4 4200 Go", 64},
        {"GeForce4 440 Go", 64},
        {"GeForce4 460 Go", 64},
        {"GeForce4 MX 420", 64},
        {"GeForce4 MX 440", 64},
        {"GeForce4 MX 460", 64},
        {"GeForce4 MX", 64},
        {"GeForce4 Ti 4200", 64},
        {"GeForce4 Ti 4400", 128},
        {"GeForce4 Ti 4600", 128},
        {"GeForce4 Ti 4800", 128},
        {"GeForce4", 64},
        {"GeForce FX 5200", 64},
        {"GeForce FX 5600", 128},
        {"GeForce FX 5800 Ultra", 128},
        {"GeForce FX 5800", 128},
        {"GeForce FX 5900 Ultra", 128},
        {"GeForce FX 5900", 128},
        {"Quadro FX 1000", 128},
        {"Quadro FX", 128},
        {"Quadro4 500", 64},
        {"Quadro4 700", 64},
        {"Quadro4 900", 64},
        {"Quadro4", 64},
        {"Quadro2 Pro", 32},
        {"Quadro2 MXR", 32},
        {"Quadro2", 32},
        {"Quadro DCC", 32},
        {"Quadro", 32},
        {"Matrox ICD for Parhelia", 128}
    };

    for (size_t index = 0;
         index < sizeof(estimates) / sizeof(estimates[0]);
         ++index) {
        const size_t prefixLength = strlen(estimates[index].rendererPrefix);
        if (coduo_crt_strnicmp(
                renderer, estimates[index].rendererPrefix,
                prefixLength) == 0) {
            return (uint32_t)estimates[index].memoryMegabytes * 1024u * 1024u;
        }
    }
    return 0;
}
#endif

/* Source: CoDUOMP.exe 0x004698e0..0x00469958.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004698e0_00469959.mcode.
 * Role name: the result is stored at 0x009cf1e4, printed as video-card MB,
 * and used as sys_vidMB. The original tries DirectDraw directly and through
 * adapter enumeration before the OpenGL renderer-name fallback.
 *
 * NOT_FROM_ORIGINAL_SOURCE: the SDL application shell has no DirectDraw or
 * WGL device. Use the dedicated-adapter DXGI measurement on Windows and the
 * recovered renderer-name table through a hidden SDL OpenGL context. Unknown
 * hardware reports the shipped recommendation table's highest tier so the
 * selected configure_mp.csv row remains the top-quality profile.
 *
 * The final value is a power-of-two MB bucket. If rounding upward overshoots
 * the measured ceiling by more than 32 MB, the previous bucket is selected. */
int32_t Sys_GetVideoMemoryMB(void)
{
    uint32_t memoryBytes = 0;

#if defined(_WIN32)
    memoryBytes = coduomp_windows_query_dedicated_video_memory();
    if (memoryBytes == 0) {
        char renderer[256];

        if (coduomp_sdl_probe_renderer_string_compat(
                renderer, sizeof(renderer)) != qfalse) {
            memoryBytes = coduomp_estimate_video_memory_bytes_from_renderer(
                renderer);
        }
    }
#endif
    if (memoryBytes == 0)
        return CODUOMP_VIDEO_MEMORY_PROFILE_MB;

    const uint32_t measuredMegabytes =
        ((memoryBytes - 1u) >> 20) + 1u;
    uint32_t bucketMegabytes = 1;
    while (bucketMegabytes < measuredMegabytes)
        bucketMegabytes <<= 1;
    if (bucketMegabytes - measuredMegabytes > 32u)
        bucketMegabytes >>= 1;
    return (int32_t)bucketMegabytes;
}
