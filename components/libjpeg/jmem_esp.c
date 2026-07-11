/*
 * ESP-IDF memory backend for the vendored IJG libjpeg (replaces jmemnobs.c,
 * which is deliberately NOT vendored). Everything goes to PSRAM: a progressive
 * decode holds the whole-image DCT coefficient arrays (~2.5 MB for a ~1 MP
 * photo), which must never land in the scarce internal DRAM. No backing store
 * — if PSRAM runs out, the library fails the decode cleanly via its error
 * manager instead of paging.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jmemsys.h"
#include "esp_heap_caps.h"

GLOBAL(void *)
jpeg_get_small (j_common_ptr cinfo, size_t sizeofobject)
{
  (void)cinfo;
  return heap_caps_malloc(sizeofobject, MALLOC_CAP_SPIRAM);
}

GLOBAL(void)
jpeg_free_small (j_common_ptr cinfo, void * object, size_t sizeofobject)
{
  (void)cinfo; (void)sizeofobject;
  heap_caps_free(object);
}

GLOBAL(void FAR *)
jpeg_get_large (j_common_ptr cinfo, size_t sizeofobject)
{
  (void)cinfo;
  return heap_caps_malloc(sizeofobject, MALLOC_CAP_SPIRAM);
}

GLOBAL(void)
jpeg_free_large (j_common_ptr cinfo, void FAR * object, size_t sizeofobject)
{
  (void)cinfo; (void)sizeofobject;
  heap_caps_free(object);
}

/* Advertise what PSRAM can actually still give us, so jmemmgr sizes its pools
 * realistically instead of trusting a fixed max_memory_to_use guess. */
GLOBAL(long)
jpeg_mem_available (j_common_ptr cinfo, long min_bytes_needed,
                    long max_bytes_needed, long already_allocated)
{
  (void)cinfo; (void)min_bytes_needed; (void)already_allocated;
  size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  /* Keep 1 MB of PSRAM headroom for the rest of the firmware. */
  size_t usable = (free_psram > 1024 * 1024) ? free_psram - 1024 * 1024 : 0;
  return (usable > (size_t)max_bytes_needed) ? max_bytes_needed : (long)usable;
}

/* No backing store on this target: jmemmgr only calls this when the image
 * doesn't fit in memory, and then this error fails the decode cleanly. */
GLOBAL(void)
jpeg_open_backing_store (j_common_ptr cinfo, backing_store_ptr info,
                         long total_bytes_needed)
{
  (void)info; (void)total_bytes_needed;
  ERREXIT(cinfo, JERR_NO_BACKING_STORE);
}

GLOBAL(long)
jpeg_mem_init (j_common_ptr cinfo)
{
  (void)cinfo;
  return 0;   /* default max_memory_to_use = 0 → jmemmgr asks jpeg_mem_available */
}

GLOBAL(void)
jpeg_mem_term (j_common_ptr cinfo)
{
  (void)cinfo;
}
