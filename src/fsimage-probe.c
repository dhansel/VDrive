/*
 * fsimage-probe.c - Probe disk images.
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include <stdio.h>
#include <string.h>

#include "archdep.h"
#include "diskconstants.h"
#include "diskimage.h"
#include "gcr.h"
#include "fsimage-gcr.h"
#include "fsimage-p64.h"
#include "fsimage-probe.h"
#include "fsimage.h"
#include "lib.h"
#include "log.h"
#include "types.h"
#include "util.h"
#include "x64.h"

#define IS_D67_LEN(x) ((x) == D67_FILE_SIZE)
#define IS_D71_LEN(x) (((x) == D71_FILE_SIZE) || ((x) == D71_FILE_SIZE_E))
#define IS_D81_LEN(x) (((x) == D81_FILE_SIZE) || ((x) == D81_FILE_SIZE_E) || \
                       ((x) == D81_FILE_SIZE_81) || ((x) == D81_FILE_SIZE_81E) || \
                       ((x) == D81_FILE_SIZE_82) || ((x) == D81_FILE_SIZE_82E) || \
                       ((x) == D81_FILE_SIZE_83) || ((x) == D81_FILE_SIZE_83E))
#define IS_D80_LEN(x) ((x) == D80_FILE_SIZE)
#define IS_D82_LEN(x) ((x) == D82_FILE_SIZE)
#define IS_D1M_LEN(x) (((x) == D1M_FILE_SIZE) || ((x) == D1M_FILE_SIZE_E))
#define IS_D2M_LEN(x) (((x) == D2M_FILE_SIZE) || ((x) == D2M_FILE_SIZE_E))
#define IS_D4M_LEN(x) (((x) == D4M_FILE_SIZE) || ((x) == D4M_FILE_SIZE_E))

static log_t disk_image_probe_log = LOG_DEFAULT;

static void disk_image_check_log(disk_image_t *image, const char *type)
{
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    log_verbose(LOG_DEFAULT, "%s disk image recognised: %s, %u tracks%s",
                type, fsimage->name, image->tracks,
                image->read_only ? " (read only)." : ".");
}

static int disk_image_check_min_block(unsigned int blk, unsigned int length)
{
    if (blk < length) {
        log_error(disk_image_probe_log, "Cannot read block %u.", blk);
        return -1;
    }
    return 0;
}


static unsigned int disk_image_get_size_blocks(fsimage_t *fsimage, int max_blocks)
{
  unsigned int blk = 0;

#if 0 && defined(WIN32)
  size_t len;
  uint8_t block[256];

  // why not trust archdep_file_size? this is slow on lower-powered platforms
  // and/or for images accessed over network connection
  archdep_frewind(fsimage->fd);
  while ((len = archdep_fread(block, 1, 256, fsimage->fd)) == 256) {
    if (++blk > max_blocks) {
      log_error(disk_image_probe_log, "Disk image too large.");
      break;
    }
  }
#else
  blk = archdep_file_size(fsimage->fd) / 256;
  if( blk > max_blocks ) 
    {
      blk = max_blocks+1;
      log_error(disk_image_probe_log, "Disk image too large.");
    }
#endif
  
  return blk;
}

static int disk_image_check_for_d64(disk_image_t *image)
{
    /*** detect 35..42 track d64 image, determine image parameters.
         Walk from 35 to 42, calculate expected image file size for each track,
         and compare this with the size of the given image. */

    int checkimage_tracks, checkimage_errorinfo;
    size_t countbytes, checkimage_blocks;
    off_t checkimage_realsize;
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    checkimage_errorinfo = 0;

    checkimage_realsize = archdep_file_size(fsimage->fd);
    checkimage_tracks = NUM_TRACKS_1541; /* start at track 35 */
    checkimage_blocks = D64_FILE_SIZE_35 / 256;

    while (1) {
        /* check if image matches "checkimage_tracks" */
        if (checkimage_realsize == checkimage_blocks * 256) {
            /* image file matches size-with-no-error-info */
            checkimage_errorinfo = 0;
            break;
        } else if (checkimage_realsize == checkimage_blocks * 256 + checkimage_blocks) {
            /* image file matches size-with-error-info */
            checkimage_errorinfo = 1;
            break;
        }

        /* try next track (all tracks from 35 to 42 have 17 blocks) */
        checkimage_tracks++;
        checkimage_blocks += 17;

        /* we tried them all up to 42, none worked, image must be corrupt */
        if (checkimage_tracks > MAX_TRACKS_1541) {
            return 0;
        }
    }

    /*** set parameters in image structure, read error info */
    image->type = DISK_IMAGE_TYPE_D64;
    image->tracks = checkimage_tracks;
    image->max_half_tracks = MAX_TRACKS_1541 * 2;

    if (checkimage_errorinfo) {
        fsimage->error_info.map = lib_calloc(1, checkimage_blocks);
        fsimage->error_info.len = (int)checkimage_blocks;
        if (util_fpread(fsimage->fd, fsimage->error_info.map, checkimage_blocks, 256 * checkimage_blocks) < 0) {
            return 0;
        }
    }

    /*** log and return successfully */
    disk_image_check_log(image, "D64");
    return 1;
}


static int disk_image_check_for_d67(disk_image_t *image)
{
    unsigned int blk = 0;
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    if (!(IS_D67_LEN(archdep_file_size(fsimage->fd)))) {
        return 0;
    }

    image->type = DISK_IMAGE_TYPE_D67;
    image->tracks = NUM_TRACKS_2040;
    image->max_half_tracks = MAX_TRACKS_2040 * 2;

    blk = disk_image_get_size_blocks(fsimage, NUM_BLOCKS_2040);
    if (disk_image_check_min_block(blk, NUM_BLOCKS_2040) < 0) {
        return 0;
    }

    switch (blk) {
        case NUM_BLOCKS_2040:
            image->tracks = NUM_TRACKS_2040;
            image->max_half_tracks = MAX_TRACKS_2040 * 2;
            break;
        default:
            return 0;
    }
    disk_image_check_log(image, "D67");
    return 1;
}

static int disk_image_check_for_d71(disk_image_t *image)
{
    unsigned int blk = 0;
    fsimage_t *fsimage;
    size_t checkimage_realsize;
    int checkimage_errorinfo;

    fsimage = image->media.fsimage;
    checkimage_realsize = archdep_file_size(fsimage->fd);
    checkimage_errorinfo = 0;

    if (!(IS_D71_LEN(checkimage_realsize))) {
        return 0;
    }

    checkimage_errorinfo = (checkimage_realsize == D71_FILE_SIZE_E) ? 1 : 0;

    image->type = DISK_IMAGE_TYPE_D71;
    image->tracks = NUM_TRACKS_1571;
    image->max_half_tracks = MAX_TRACKS_1571 * 2;

    blk = disk_image_get_size_blocks(fsimage, NUM_BLOCKS_1571);
    if (disk_image_check_min_block(blk, NUM_BLOCKS_1571) < 0) {
        return 0;
    }

    if (checkimage_errorinfo) {
        fsimage->error_info.map = lib_calloc(1, blk);
        fsimage->error_info.len = blk;
        if (util_fpread(fsimage->fd, fsimage->error_info.map, blk, 256 * blk) < 0) {
            return 0;
        }
    }

    disk_image_check_log(image, "D71");
    return 1;
}

static int disk_image_check_for_d81(disk_image_t *image)
{
    unsigned int blk = 0;
    char *ext;
    fsimage_t *fsimage;
    int checkimage_errorinfo;
    unsigned int checkimage_blocks;

    fsimage = image->media.fsimage;

    if (!(IS_D81_LEN(archdep_file_size(fsimage->fd)))) {
        return 0;
    }

    /* .d1m images share the same sizes with .d81, so we reject based on the
       file extension what is likely a .d1m image */
    ext = util_get_extension(fsimage->name);
    if (ext && ext[0] && (ext[1] == '1') && ext[2]) {
        return 0;
    }

    archdep_frewind(fsimage->fd);

    blk = disk_image_get_size_blocks(fsimage, MAX_BLOCKS_1581 + 13);
    if (disk_image_check_min_block(blk, NUM_BLOCKS_1581) < 0) {
        return 0;
    }

    switch (blk) {
        case NUM_BLOCKS_1581:          /* 80 tracks */
        case NUM_BLOCKS_1581 + 12:     /* 80 tracks, with errors */
            image->tracks = NUM_TRACKS_1581;
            break;
        case NUM_BLOCKS_1581 + 40:     /* 81 tracks */
        case NUM_BLOCKS_1581 + 40 + 12: /* 81 tracks, with errors */
            image->tracks = NUM_TRACKS_1581 + 1;
            break;
        case NUM_BLOCKS_1581 + 80:     /* 82 tracks */
        case NUM_BLOCKS_1581 + 80 + 12: /* 82 tracks, with errors */
            image->tracks = NUM_TRACKS_1581 + 2;
            break;
        case NUM_BLOCKS_1581 + 120:    /* 83 tracks */
        case NUM_BLOCKS_1581 + 120 + 12: /* 83 tracks, with errors */
            image->tracks = NUM_TRACKS_1581 + 3;
            break;
        default:
            return 0;
    }

    image->type = DISK_IMAGE_TYPE_D81;
    image->max_half_tracks = MAX_TRACKS_1581 * 2;

    switch (blk) {
        case NUM_BLOCKS_1581 + 12:     /* 80 tracks, with errors */
        case NUM_BLOCKS_1581 + 40 + 12: /* 81 tracks, with errors */
        case NUM_BLOCKS_1581 + 80 + 12: /* 82 tracks, with errors */
        case NUM_BLOCKS_1581 + 120 + 12: /* 83 tracks, with errors */
            checkimage_errorinfo = 1;
            break;
        default:
            checkimage_errorinfo = 0;
            break;
    }

    if (checkimage_errorinfo) {
        checkimage_blocks = image->tracks * 40;
        fsimage->error_info.map = lib_calloc(1, checkimage_blocks);
        fsimage->error_info.len = checkimage_blocks;
        if (util_fpread(fsimage->fd, fsimage->error_info.map, checkimage_blocks, 256 * checkimage_blocks) < 0) {
            return 0;
        }
    }
    disk_image_check_log(image, "D81");
    return 1;
}

static int disk_image_check_for_d80(disk_image_t *image)
{
    unsigned int blk = 0;
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    if (!(IS_D80_LEN(archdep_file_size(fsimage->fd)))) {
        return 0;
    }

    image->type = DISK_IMAGE_TYPE_D80;
    image->tracks = NUM_TRACKS_8050;
    image->max_half_tracks = MAX_TRACKS_8050 * 2;

    blk = disk_image_get_size_blocks(fsimage, NUM_BLOCKS_8050 + 6);
    if (disk_image_check_min_block(blk, NUM_BLOCKS_8050) < 0) {
        return 0;
    }

    switch (blk) {
        case NUM_BLOCKS_8050:
            image->tracks = NUM_TRACKS_8050;
            image->max_half_tracks = MAX_TRACKS_8050 * 2;
            break;
        default:
            return 0;
    }
    disk_image_check_log(image, "D80");
    return 1;
}

static int disk_image_check_for_d82(disk_image_t *image)
{
    unsigned int blk = 0;
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    if (!(IS_D82_LEN(archdep_file_size(fsimage->fd)))) {
        return 0;
    }

    image->type = DISK_IMAGE_TYPE_D82;
    image->tracks = NUM_TRACKS_8250;
    image->max_half_tracks = MAX_TRACKS_8250 * 2;

    blk = disk_image_get_size_blocks(fsimage, NUM_BLOCKS_8250 + 6);
    if (disk_image_check_min_block(blk, NUM_BLOCKS_8250) < 0) {
        return 0;
    }

    switch (blk) {
        case NUM_BLOCKS_8250:
            image->tracks = NUM_TRACKS_8250;
            image->max_half_tracks = MAX_TRACKS_8250 * 2;
            break;
        default:
            return 0;
    }
    disk_image_check_log(image, "D82");
    return 1;
}

#ifdef HAVE_X64_IMAGE
static int disk_image_check_for_x64(disk_image_t *image)
{
    uint8_t header[X64_HEADER_LENGTH];
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    archdep_frewind(fsimage->fd);

    if (archdep_fread(header, X64_HEADER_LENGTH, 1, fsimage->fd) < 1) {
        return 0;
    }

    if (header[X64_HEADER_MAGIC_OFFSET + 0] != X64_HEADER_MAGIC_1 ||
        header[X64_HEADER_MAGIC_OFFSET + 1] != X64_HEADER_MAGIC_2 ||
        header[X64_HEADER_MAGIC_OFFSET + 2] != X64_HEADER_MAGIC_3 ||
        header[X64_HEADER_MAGIC_OFFSET + 3] != X64_HEADER_MAGIC_4) {
        return 0;
    }

    if (header[X64_HEADER_FLAGS_OFFSET + 1] > MAX_TRACKS_1541) {
        return 0;
    }

    image->type = DISK_IMAGE_TYPE_X64;
    image->tracks = header[X64_HEADER_FLAGS_OFFSET + 1];
    image->max_half_tracks = MAX_TRACKS_1541 * 2;

    disk_image_check_log(image, "X64");
    return 1;
}
#endif

static int disk_image_check_for_gcr(disk_image_t *image)
{
#if 0
    /* if 0'ed because of:
       'if (max_track_length > NUM_MAX_MEM_BYTES_TRACK) {'
       further down below
    */
    WORD max_track_length;
#endif
    uint8_t header[32];
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    if (util_fpread(fsimage->fd, header, sizeof (header), 0) < 0) {
        log_error(disk_image_probe_log, "Cannot read image header.");
        return 0;
    }

    if (header[8] != 0) {
        log_error(disk_image_probe_log,
                  "Import GCR: Unknown GCR image version %i.",
                  (int)header[8]);
        return 0;
    }

/* used to be
   if (header[9] < 1 || header[9] > MAX_GCR_TRACKS * 2) {
   however, header[] is of type BYTE and MAX_GCR_TRACKS is 140
*/
    if (header[9] < 1) {
        log_error(disk_image_probe_log,
                  "Import GCR: Invalid number of tracks (%i).",
                  (int)header[9]);
        return 0;
    }

#if 0
    /* if 0'ed because:
       max_track_length is of type WORD and NUM_MAX_MEM_BYTES_TRACK is 65536
     */
    max_track_length = util_le_buf_to_word(&header[10]);
    if (max_track_length > NUM_MAX_MEM_BYTES_TRACK) {
        log_error(disk_image_probe_log, "Too large max track length.");
        return 0;
    }
#endif

    if (!strncmp("GCR-1541", (char*)header, 8)) {
        image->type = DISK_IMAGE_TYPE_G64;
    } else if (!strncmp("GCR-1571", (char*)header, 8)) {
        image->type = DISK_IMAGE_TYPE_G71;
    } else {
        return 0;
    }

    image->tracks = header[9] / 2;
    image->max_half_tracks = header[9];
    disk_image_check_log(image, "GCR");
    return 1;
}

static int disk_image_check_for_p64(disk_image_t *image)
{
    uint8_t header[8];
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    if (util_fpread(fsimage->fd, header, sizeof(header), 0) < 0) {
        log_error(disk_image_probe_log, "Cannot read image header.");
        return 0;
    }

    if (strncmp("P64-1541", (char*)header, 8)) {
        return 0;
    }

    /*log_error(disk_image_probe_log, "P64 detected"); */

    image->type = DISK_IMAGE_TYPE_P64;
    image->tracks = MAX_TRACKS_1541;
    image->max_half_tracks = MAX_TRACKS_1541 * 2;
    disk_image_check_log(image, "P64");

    if (image->p64 != NULL) {
        if (fsimage_read_p64_image(image) < 0) {
            return 0;
        }
    }
    return 1;
}

static int disk_image_check_for_d1m(disk_image_t *image)
{
    unsigned int blk = 0;
    char *ext;
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    /* reject files with unknown size */
    if (!(IS_D1M_LEN(archdep_file_size(fsimage->fd)))) {
        return 0;
    }

    /* .d81 images share the same sizes with .d1m, so we reject based on the
       file extension what is likely a .d81 image */
    ext = util_get_extension(fsimage->name);
    if (ext && ext[0] && (ext[1] == '8') && ext[2] == '1') {
        return 0;
    }

    image->type = DISK_IMAGE_TYPE_D1M;
    image->tracks = NUM_TRACKS_1000;
    image->max_half_tracks = MAX_TRACKS_1000 * 2;

    blk = disk_image_get_size_blocks(fsimage, NUM_BLOCKS_1000 + 13);
    if (disk_image_check_min_block(blk, NUM_BLOCKS_1000) < 0) {
        return 0;
    }

    switch (blk) {
        case NUM_BLOCKS_1000:
        case NUM_BLOCKS_1000 + 12: /* with errors */
            image->tracks = NUM_TRACKS_1000;
            image->max_half_tracks = MAX_TRACKS_1000 * 2;
            break;
        default:
            return 0;
    }
    disk_image_check_log(image, "D1M");
    return 1;
}

static int disk_image_check_for_d2m(disk_image_t *image)
{
    unsigned int blk = 0;
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    if (!(IS_D2M_LEN(archdep_file_size(fsimage->fd)))) {
        return 0;
    }

    image->type = DISK_IMAGE_TYPE_D2M;
    image->tracks = NUM_TRACKS_2000;
    image->max_half_tracks = MAX_TRACKS_2000 * 2;

    blk = disk_image_get_size_blocks(fsimage, NUM_BLOCKS_2000 + 26);
    if (disk_image_check_min_block(blk, NUM_BLOCKS_2000) < 0) {
        return 0;
    }

    switch (blk) {
        case NUM_BLOCKS_2000:
        case NUM_BLOCKS_2000 + 25: /* with errors */
            image->tracks = NUM_TRACKS_2000;
            image->max_half_tracks = MAX_TRACKS_2000 * 2;
            break;
        default:
            return 0;
    }
    disk_image_check_log(image, "D2M");
    return 1;
}

static int disk_image_check_for_d4m(disk_image_t *image)
{
    unsigned int blk = 0;
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;
    image->tracks = NUM_TRACKS_2000;

    if (!(IS_D4M_LEN(archdep_file_size(fsimage->fd)))) {
        return 0;
    }

    image->type = DISK_IMAGE_TYPE_D4M;
    image->tracks = NUM_TRACKS_4000;
    image->max_half_tracks = MAX_TRACKS_4000 * 2;

    blk = disk_image_get_size_blocks(fsimage, NUM_BLOCKS_4000 + 51);
    if (disk_image_check_min_block(blk, NUM_BLOCKS_4000) < 0) {
        return 0;
    }

    switch (blk) {
        case NUM_BLOCKS_4000:
        case NUM_BLOCKS_4000 + 50: /* with errors */
            image->tracks = NUM_TRACKS_4000;
            image->max_half_tracks = MAX_TRACKS_4000 * 2;
            break;
        default:
            return 0;
    }
    disk_image_check_log(image, "D4M");
    return 1;
}

static int disk_image_check_for_dhd(disk_image_t *image)
{
    off_t blk = 0;
    uint8_t sector[512];
    fsimage_t *fsimage;
    off_t pos;
    unsigned char hdmagic[16] = {0x43, 0x4d, 0x44, 0x20, 0x48, 0x44, 0x20, 0x20,
        0x8d, 0x03, 0x88, 0x8e, 0x02, 0x88, 0xea, 0x60};

    fsimage = image->media.fsimage;
    image->tracks = 65535;

    blk = archdep_file_size(fsimage->fd);

    /* only allow blank images to be attached if the CMDHD rom is loaded */
    if (blk == 0) {
#if 0
        if (!machine_drive_rom_check_loaded(DISK_IMAGE_TYPE_DHD)) {
            goto good;
        }
        log_error(disk_image_probe_log,
                  "Sorry, you can't attach an empty DHD image unless " \
                  "the CMDHD boot ROM is loaded.");
#else
        log_error(disk_image_probe_log,
                  "Sorry, you can't attach an empty DHD image.");
#endif
        return 0;
    }

    /* next make sure the file is a multiple of 256 bytes and greater than
       equal 73728 bytes (which is the smallest possible running DHD image */
    /* we used to look for multiples of 512 bytes, but writes of 256 bytes
       to expanding images in vdrive might make this fail. */
    /* FIXME: perhaps this can be made more strict to prevent false positives? */
    if ((blk % 256 != 0) || ( blk < 73728 )) {
        return 0;
    }

#if 0
    /* since the size check(s) are weak, check CRT header to prevent CRT files
       being detected as DHD images (bug #1489). having a crt header at the start
       of a DHD container seems unlikely enough for this to work fine. */
    if (crt_getid(image->media.fsimage->name) >= 0) {
        log_error(disk_image_probe_log, "trying to attach a CRT file as DHD image, aborting.");
        return 0;
    }
#endif
    /* FIXME: perhaps other headers (g64, t64, p64...) need to be checked here */

#if 0
    /* if the CMDHD rom is loaded, allow it regardless */
    if (!machine_drive_rom_check_loaded(DISK_IMAGE_TYPE_DHD)) {
        goto good;
    }
#endif

    /* at this point, make sure the image is good for vdrive */

    /* look for configuration block */
    archdep_frewind(fsimage->fd);
    /* start at LBA 2 or 1024 */
    pos = 1024;

    while ( pos < blk ) {
        if (archdep_fseek(fsimage->fd, pos, SEEK_SET)) {
            /* hit the end of file */
            break;
        }
        if (archdep_fread(sector, 512, 1, fsimage->fd) != 1) {
            /* hit the end of file */
            break;
        }
        /* otherwise check the cmd sig */
        if ( memcmp(&(sector[0x1f0]), hdmagic, 16) == 0 ) {
            goto good;
        }
        /* try next 128 sectors of 64 KiB bytes */
        pos += 65536;
    }
    /* hit the end of file */

    /* no good */
    return 0;

good:
    /* image is allowed */
    image->type = DISK_IMAGE_TYPE_DHD;
    image->max_half_tracks = 0;

    disk_image_check_log(image, "DHD");
    return 1;
}

static int disk_image_check_for_d90(disk_image_t *image)
{
    off_t blk = 0;
    fsimage_t *fsimage;

    fsimage = image->media.fsimage;

    /* get file size */
    blk = archdep_file_size(fsimage->fd);

    /* only allow true D9090/D9060 image sizes right now */
    if (blk == D9060_FILE_SIZE) {
        /* D9060 has 4 heads */
        image->sectors = 4 * 32;
    } else if (blk == D9090_FILE_SIZE) {
        /* D9090 has 6 heads */
        image->sectors = 6 * 32;
    } else {
        return 0;
    }

    /* set max track, for now; it starts at 0 */
    image->tracks = 152;

    /* image is allowed */
    image->type = DISK_IMAGE_TYPE_D90;
    image->max_half_tracks = 0;

    disk_image_check_log(image, "D90");
    return 1;
}

int fsimage_probe(disk_image_t *image)
{
    if (disk_image_check_for_d64(image)) {
        return 0;
    }
    if (disk_image_check_for_d67(image)) {
        return 0;
    }
    if (disk_image_check_for_d71(image)) {
        return 0;
    }
    if (disk_image_check_for_d81(image)) {
        return 0;
    }
    if (disk_image_check_for_d80(image)) {
        return 0;
    }
    if (disk_image_check_for_d82(image)) {
        return 0;
    }
    if (disk_image_check_for_p64(image)) {
        return 0;
    }
    if (disk_image_check_for_gcr(image)) {
        return 0;
    }
#ifdef HAVE_X64_IMAGE
    if (disk_image_check_for_x64(image)) {
        return 0;
    }
#endif
    if (disk_image_check_for_d1m(image)) {
        return 0;
    }
    if (disk_image_check_for_d2m(image)) {
        return 0;
    }
    if (disk_image_check_for_d4m(image)) {
        return 0;
    }
    if (disk_image_check_for_d90(image)) {
        return 0;
    }
    if (disk_image_check_for_dhd(image)) {
        return 0;
    }

    return -1;
}

void fsimage_probe_init(void)
{
    disk_image_probe_log = log_open("Filesystem Image Probe");
}
