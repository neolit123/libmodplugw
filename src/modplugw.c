/* modplugw.c
 *
 * authors:
 *     Lubomir I. Ivanov, 2014 and later
 *
 * this source file is released in the public domain without warranty of any kind
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

#include <modplugw.h>

#define VERBOSE_TITLE "(modplugw_decode) "
#define ALLOC_INCREMENT (1 << 22)

float
modplugw_bytes_to_sec(modplugw_desc_t *desc, unsigned int bytes)
{
	if (!bytes || !desc || !desc->sample_size || !desc->sample_rate)
		return 0.0;
	return (float)(bytes / desc->sample_size) / (float)desc->sample_rate;
}

char *
modplugw_get_pattern_offset(modplugw_desc_t *desc, const unsigned int pattern)
{
	if (!desc)
		return NULL;
	if (pattern > desc->npatterns)
		return NULL;
	return &desc->data[desc->pattern[pattern]];
}

char *
modplugw_get_end_pattern_offset(modplugw_desc_t *desc)
{
	if (!desc || !desc->allocated)
		return NULL;
	return modplugw_get_pattern_offset(desc, desc->npatterns);
}

unsigned int
modplugw_get_len_between_patterns(
	modplugw_desc_t *desc,
	const unsigned int start_pat,
	const unsigned int end_pat)
{
	if (!desc || start_pat >= end_pat || end_pat > desc->npatterns)
		return 0;
	char *start = modplugw_get_pattern_offset(desc, start_pat);
	char *end = modplugw_get_pattern_offset(desc, end_pat);
	return end - start;
}

unsigned int
modplugw_get_pattern_len(
	modplugw_desc_t *desc,
	const unsigned int start_pat
)
{
	if (!desc || start_pat > desc->npatterns)
		return 0;
	const unsigned int end_pat = start_pat + 1;
	return modplugw_get_len_between_patterns(desc, start_pat, end_pat);
}

unsigned int
modplugw_get_pattern_rows(
	modplugw_desc_t *desc,
	const unsigned int start_pat
)
{
	if (!desc || start_pat > desc->npatterns)
		return 0;
	return desc->nrows[start_pat];
}

void
modplugw_free_desc(modplugw_desc_t *desc)
{
	if (!desc || !desc->allocated)
		return;
	if (desc->mod)
		ModPlug_Unload(desc->mod);
	free(desc->pattern);
	free(desc->data);
	memset(desc, 0, sizeof(modplugw_desc_t));
	free(desc);
}

modplugw_desc_t *
modplugw_decode(
	const char *buf,
	const unsigned int len,
	ModPlug_Settings *settings,
	int verbose
)
{
	if (!buf || !len)
		return NULL;

	// allocate memory for the descriptor
	modplugw_desc_t *desc = (modplugw_desc_t *)malloc(sizeof(modplugw_desc_t));
	if (!desc)
		return NULL;
	memset(desc, 0, sizeof(modplugw_desc_t));
	desc->allocated = 1;

	// some predefined settings
	if (!settings) {
		ModPlug_Settings modSettings;
		ModPlug_GetSettings(&modSettings);
		modSettings.mFlags |= MODPLUG_ENABLE_OVERSAMPLING;
		modSettings.mStereoSeparation = 256;
		modSettings.mLoopCount = 0;
		modSettings.mChannels = 2;
		modSettings.mBits = 16;
		modSettings.mFrequency = 44100;
		modSettings.mResamplingMode = MODPLUG_RESAMPLE_LINEAR;
		settings = &modSettings;
	}

	// set settings, load mod, and set volume
	ModPlug_SetSettings(settings);
	ModPlugFile *mod = ModPlug_Load(buf, len);
	ModPlug_SetMasterVolume(mod, 196);
	desc->mod = mod;

	// set sample size and read buffer properties
	const unsigned int sample_size = settings->mChannels * (settings->mBits / 8);
	const unsigned int alloc_increment = 1 << 22; // ~4 megs
	unsigned int buf_size = settings->mFrequency * sample_size * (unsigned int)(ModPlug_GetLength(mod) * 0.001f);

	// sample
	desc->sample_size = sample_size;
	desc->sample_rate = settings->mFrequency;
	char s[sample_size];

	// allocate memory for the patterns, the 'pattern' field contains one extra pattern for the end bytes
	const unsigned int npatterns = ModPlug_NumPatterns(mod);
	desc->npatterns = npatterns;
	desc->pattern = (unsigned int *)malloc((npatterns + 1) * sizeof(unsigned int));
	desc->nrows = (unsigned int *)malloc((npatterns + 1) * sizeof(unsigned int));

	// check for zero patterns or zero sample size
	if (!npatterns || !sample_size)
		goto error_alloc;

	// the sample buffer to write to
	char *samples = (char *)malloc(buf_size);

	// alloc checks
	if (!samples || !desc->pattern || !desc->nrows)
		goto error_alloc;

	int cur_order = 0, last_order = -1, read = 0, bytes_total = 0, row = 0;

	if (verbose) {
		printf(VERBOSE_TITLE "sample_size: %u\n", sample_size);
		printf(VERBOSE_TITLE "buf_size: %u\n", buf_size);
		printf(VERBOSE_TITLE "processing %u patterns: ", npatterns);
	}

	// read all song bytes into the local buffer
	while ((read = ModPlug_Read(mod, &s, sample_size))) {
		// set pattern position
		cur_order = ModPlug_GetCurrentOrder(mod);
		if (cur_order != last_order) {
			if (verbose && cur_order < npatterns)
				printf("%u ", cur_order);
			if (cur_order > 0)
				desc->nrows[cur_order - 1] = row + 1;
			desc->pattern[cur_order] = bytes_total;
			last_order = cur_order;
		}

		// copy sample
		memcpy(&samples[bytes_total], &s, sample_size);
		bytes_total += read;

		// allocate more memory
		if (bytes_total >= buf_size) {
			buf_size += alloc_increment;
			samples = (char *)realloc(samples, buf_size);
			if (!samples)
				goto error_alloc;
		}

		// get current row in pattern
		row = ModPlug_GetCurrentRow(mod);
	}

	// set the last pattern to zero rows
	desc->nrows[npatterns] = 0;

	// get the length of a single row
	int i = 0;
	while (!desc->nrows[i] && i < npatterns)
		i++;
	desc->row_len = desc->pattern[i + 1] / desc->nrows[i];

	// print pattern info
	if (verbose) {
		puts("");
		i = 0;
		while (i < npatterns) {
			printf(VERBOSE_TITLE "pattern: %u, rows: %u, offset: %u\n", i, desc->nrows[i], desc->pattern[i]);
			i++;
		}
		printf(VERBOSE_TITLE "row_len: %u\n", desc->row_len);
	}

	// truncate the mem block if needed
	if (buf_size != bytes_total) {
		samples = (char *)realloc(samples, bytes_total);
		if (!samples)
			goto error_alloc;
	}
	desc->data = samples;
	desc->len = bytes_total;

	if (verbose)
		printf(VERBOSE_TITLE "bytes_total: %u\n", bytes_total);
	return desc;

	error_alloc:
		modplugw_free_desc(desc);
		return NULL;
}

modplugw_seg_t *
modplugw_alloc_pattern_segment(
	modplugw_desc_t *desc,
	const unsigned int start_pat,
	const unsigned int end_pat
)
{
	if (!desc)
		return NULL;
	modplugw_seg_t *seg = (modplugw_seg_t *)malloc(sizeof(modplugw_seg_t));
	if (!seg)
		return NULL;

	seg->data = modplugw_get_pattern_offset(desc, start_pat);
	seg->len = modplugw_get_len_between_patterns(desc, start_pat, end_pat);

	seg->id = MODPLUGW_DEF_SEGMENT_ID;
	return seg;
}

modplugw_seg_t *
modplugw_alloc_row_segment(
	modplugw_desc_t *desc,
	const unsigned int start_pat,
	const unsigned int start_row,
	const unsigned int end_pat,
	const unsigned int end_row
)
{
	if (!desc ||
	    start_pat > desc->npatterns || start_pat > desc->npatterns || start_pat > end_pat ||
	    start_row > desc->nrows[start_pat] - 1 || end_row > desc->nrows[end_pat] - 1) {
		return NULL;
	}

	modplugw_seg_t *seg = (modplugw_seg_t *)malloc(sizeof(modplugw_seg_t));
	if (!seg)
		return NULL;

	char *start = modplugw_get_pattern_offset(desc, start_pat);
	start += start_row * desc->row_len;
	seg->data = start;

	char *end = modplugw_get_pattern_offset(desc, end_pat);
	end += end_row * desc->row_len;
	seg->len = end - start;

	seg->id = MODPLUGW_DEF_SEGMENT_ID;
	return seg;
}

modplugw_seg_t *
modplugw_append_segments(
	modplugw_seg_t *dest,
	modplugw_seg_t **seg,
	unsigned int count
)
{
	if (!seg || !count)
		return NULL;

	// estimate the target length
	int i = 0, len = 0;;
	while (i < count) {
		len += seg[i]->len;
		i++;
	}

	// if the target is NULL allocate it
	if (!dest) {
		dest = (modplugw_seg_t *)malloc(sizeof(modplugw_seg_t));
		memset(dest, 0, sizeof(modplugw_seg_t));
		dest->id = MODPLUGW_DEF_SEGMENT_ID;
	}
	dest->data = (char *)realloc(dest->data, len);

	// append the segments
	i = 0;
	while (i < count) {
		memcpy(dest->data + dest->len, seg[i]->data, seg[i]->len);
		dest->len += seg[i]->len;
		i++;
	}
	return dest;
}

void
modplugw_free_segments(
	modplugw_seg_t **seg,
	unsigned int count
)
{
	if (!seg || !count)
		return;
	int i = 0;
	while (i < count) {
		free(seg[i]);
		i++;
	}
}
