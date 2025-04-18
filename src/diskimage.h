/*
 * diskimage.h - Common low-level disk image access.
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

#ifndef VICE_DISKIMAGE_H
#define VICE_DISKIMAGE_H

#include <sys/types.h>
#include "types.h"
#include "archdep.h"
#include "p64.h"

#define D64_FILE_SIZE_35   174848        /* D64 image, 35 tracks */
#define D64_FILE_SIZE_35E (174848 + 683) /* D64 image, 35 tracks with errors */
#define D67_FILE_SIZE      176640        /* D67 image, 35 tracks DOS1 */
#define D71_FILE_SIZE      349696        /* D71 image, 70 tracks */
#define D71_FILE_SIZE_E   (349696 + 1366)/* D71 image, 70 tracks with errors */

#define D81_FILE_SIZE      819200        /* D81 image, 80 tracks */
#define D81_FILE_SIZE_E    822400        /* D81 image, 80 tracks with errors */
#define D81_FILE_SIZE_81   829440        /* D81 image, 81 tracks (! sams as D1M) */
#define D81_FILE_SIZE_81E  832680        /* D81 image, 81 tracks with errors (! sams as D1M) */
#define D81_FILE_SIZE_82   839680        /* D81 image, 82 tracks */
#define D81_FILE_SIZE_82E  842960        /* D81 image, 82 tracks with errors */
#define D81_FILE_SIZE_83   849920        /* D81 image, 83 tracks */
#define D81_FILE_SIZE_83E  853240        /* D81 image, 83 tracks with errors */

#define D80_FILE_SIZE      533248        /* D80 image, 77 tracks */
#define D82_FILE_SIZE     1066496        /* D82 image, 77 tracks */
#define D1M_FILE_SIZE      829440        /* D1M image, 81 tracks */
#define D1M_FILE_SIZE_E    832680        /* D1M image, 81 tracks with errors */
#define D2M_FILE_SIZE     1658880        /* D2M image, 81 tracks */
#define D2M_FILE_SIZE_E   1665360        /* D2M image, 81 tracks with errors */
#define D4M_FILE_SIZE     3317760        /* D4M image, 81 tracks */
#define D4M_FILE_SIZE_E   3330720        /* D4M image, 81 tracks with errors */

#define D9090_FILE_SIZE   (153*6*32*256) /* D90 image, 153 tracks, 32 sectors, 6 heads */
#define D9060_FILE_SIZE   (153*4*32*256) /* D90 image, 153 tracks, 32 sectors, 4 heads */

#define DISK_IMAGE_DEVICE_FS   0
#define DISK_IMAGE_DEVICE_REAL 1    /* opencbm */
#define DISK_IMAGE_DEVICE_RAW  2

#ifdef HAVE_X64_IMAGE
#define DISK_IMAGE_TYPE_X64 0
#endif
#define DISK_IMAGE_TYPE_G64 100
#define DISK_IMAGE_TYPE_G71 101
#define DISK_IMAGE_TYPE_P64 200
#define DISK_IMAGE_TYPE_D64 1541
#define DISK_IMAGE_TYPE_D67 2040
#define DISK_IMAGE_TYPE_D71 1571
#define DISK_IMAGE_TYPE_D81 1581
#define DISK_IMAGE_TYPE_D80 8050
#define DISK_IMAGE_TYPE_D82 8250
#define DISK_IMAGE_TYPE_TAP 1531 /* FIXME: this is nonsense, rename and move elsewhere */
#define DISK_IMAGE_TYPE_D1M 1000
#define DISK_IMAGE_TYPE_D2M 2000
#define DISK_IMAGE_TYPE_D4M 4000
#define DISK_IMAGE_TYPE_DHD 4844 /* HD in ASCII for CMDHD */
#define DISK_IMAGE_TYPE_D90 9000 /* D9090/D9060 */

struct fsimage_s;
struct rawimage_s;
struct gcr_s;
struct disk_track_s;

struct disk_image_s {
    union media_u {
        struct fsimage_s *fsimage;
        struct rawimage_s *rawimage;
    } media;
    unsigned int read_only;
    unsigned int device; /* FS/REAL/RAW */
    unsigned int type;
    unsigned int tracks;
    unsigned int sectors; /* for D9090/D9060 */
    unsigned int max_half_tracks;
    struct gcr_s *gcr;
    TP64Image *p64;
};
typedef struct disk_image_s disk_image_t;

struct disk_addr_s {
    unsigned int track;
    unsigned int sector;
};
typedef struct disk_addr_s disk_addr_t;

void disk_image_init(void);
int disk_image_resources_init(void);
int disk_image_cmdline_options_init(void);
void disk_image_resources_shutdown(void);

void disk_image_fsimage_name_set(disk_image_t *image, const char *name);
const char *disk_image_fsimage_name_get(const disk_image_t *image);
ADFILE *disk_image_fsimage_fd_get(const disk_image_t *image);
int disk_image_fsimage_create(const char *name, unsigned int type);
int disk_image_fsimage_create_dxm(const char *name, const char *diskname, unsigned int type);
int disk_image_fsimage_create_dhd(const char *name, const char *diskname, unsigned int type);

void disk_image_rawimage_name_set(disk_image_t *image, const char *name);
void disk_image_rawimage_driver_name_set(disk_image_t *image);

void disk_image_name_set(disk_image_t *image, const char *name);
const char *disk_image_name_get(const disk_image_t *image);

disk_image_t *disk_image_create(void);
void disk_image_destroy(disk_image_t *image);

void disk_image_media_create(disk_image_t *image);
void disk_image_media_destroy(disk_image_t *image);

int disk_image_open(disk_image_t *image);
int disk_image_close(disk_image_t *image);

int disk_image_read_sector(const disk_image_t *image, uint8_t *buf, const disk_addr_t *dadr);
int disk_image_write_sector(disk_image_t *image, const uint8_t *buf, const disk_addr_t *dadr);
int disk_image_check_sector(const disk_image_t *image, unsigned int track, unsigned int sector);
unsigned int disk_image_sector_per_track(unsigned int format, unsigned int track);
unsigned int disk_image_raw_track_size(unsigned int format, unsigned int track);
unsigned int disk_image_gap_size(unsigned int format, unsigned int track);
unsigned int disk_image_header_gap_size(unsigned int format, unsigned int track);
unsigned int disk_image_sync_size(unsigned int format, unsigned int track);

int disk_image_read_image(const disk_image_t *image);
int disk_image_write_p64_image(const disk_image_t *image);
int disk_image_write_half_track(disk_image_t *image, unsigned int half_track, const struct disk_track_s *raw);

unsigned int disk_image_speed_map(unsigned int format, unsigned int track);

void disk_image_attach_log(const disk_image_t *image, signed int lognum, unsigned int unit, unsigned int drive);
void disk_image_detach_log(const disk_image_t *image, signed int lognum, unsigned int unit, unsigned int drive);
off_t disk_image_size(const disk_image_t *image);

#endif
