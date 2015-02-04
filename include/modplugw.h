/* modplugw.h
 *
 * authors:
 *     Lubomir I. Ivanov, 2014 and later
 *
 * this source file is released in the public domain without warranty of any kind
 */

/* NOTES: most methods return either 0 or NULL on failure */

#ifndef MODPLUGW_H
#define MODPLUGW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <modplug.h>

#ifdef MODPLUGW_DYNAMIC
#	define MODPLUGW_EXPORT __declspec(dllexport)
#else
#	define MODPLUGW_EXPORT
#endif

#define MODPLUGW_DEF_SEGMENT_ID -1

// a descriptor structure to be used by the extension when decoding
typedef struct {

	ModPlugFile *mod; // the modplug file reference

	unsigned int allocated; // 1 if the descriptor contains allocated data

	unsigned int *nrows; // each element holds rows per pattern. containts npatterns + 1. last one is 0 rows.
	unsigned int row_len; // the length of a single row in bytes

	unsigned int npatterns; // total patterns
	unsigned int *pattern; // each element is a pattern offsets in bytes. containts npatterns + 1

	unsigned int sample_size; // size of the a single sample in bytes (default: 16bit, 2chan = 4 bytes)
	unsigned int sample_rate; // samples per second

	char *data; // the decoded buffer
	unsigned int len; // the decoded buffer length

} modplugw_desc_t;

// a segment type with a data pointer, length of the data and an optional id
typedef struct {

	char *data;
	unsigned int len;
	int id;

} modplugw_seg_t;

// convert bytes to seconds
MODPLUGW_EXPORT
float
modplugw_bytes_to_sec(modplugw_desc_t *desc, unsigned int bytes);

// retrieve the byte offset of a pattern
MODPLUGW_EXPORT
char *
modplugw_get_pattern_offset(modplugw_desc_t *desc, const unsigned int pattern);

// retrieve the last pattern offset
MODPLUGW_EXPORT
char *
modplugw_get_end_pattern_offset(modplugw_desc_t *desc);

// retrieve the byte offset of a row
MODPLUGW_EXPORT
char *
modplugw_get_row_offset(
	modplugw_desc_t *desc,
	const unsigned int pattern,
	const unsigned int row
);

// get the length in bytes between two patterns
MODPLUGW_EXPORT
unsigned int
modplugw_get_len_between_patterns(
	modplugw_desc_t *desc,
	const unsigned int start_pat,
	const unsigned int end_pat
);

// get the length of a pattern in bytes
MODPLUGW_EXPORT
unsigned int
modplugw_get_pattern_len(modplugw_desc_t *desc, const unsigned int start_pat);

// get how many patterns there are in a row
MODPLUGW_EXPORT
unsigned int
modplugw_get_pattern_rows(
	modplugw_desc_t *desc,
	const unsigned int start_pat
);

// free the descriptor and allocated memory
MODPLUGW_EXPORT
void
modplugw_free_desc(modplugw_desc_t *desc);

// decode a buffer containing a MOD song and return a modplugw_desc_t pointer
MODPLUGW_EXPORT
modplugw_desc_t *
modplugw_decode(
	const char *buf,
	const unsigned int len,
	ModPlug_Settings *settings,
	int verbose
);

// create a segment based on a start and end pattern
MODPLUGW_EXPORT
modplugw_seg_t *
modplugw_alloc_pattern_segment(
	modplugw_desc_t *desc,
	const unsigned int start_pat,
	const unsigned int end_pat
);

// create a segment based on a start pattern row and end pattern row
MODPLUGW_EXPORT
modplugw_seg_t *
modplugw_alloc_row_segment(
	modplugw_desc_t *desc,
	const unsigned int start_pat,
	const unsigned int start_row,
	const unsigned int end_pat,
	const unsigned int end_row
);

// append a set of segments to a segment; if 'dest' is NULL a new one is allocated and returned
MODPLUGW_EXPORT
modplugw_seg_t *
modplugw_append_segments(
	modplugw_seg_t *dest,
	modplugw_seg_t **seg,
	unsigned int count
);

// free a segment
MODPLUGW_EXPORT
void
modplugw_free_segments(modplugw_seg_t **seg, unsigned int count);

#ifdef __cplusplus
}
#endif

#endif // MODPLUGW_H
