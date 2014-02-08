/*******************************************************************************
 * BEGIN COPYRIGHT NOTICE
 * 
 * This file is part of program "I-Trigue 2.1 3300 Digital Control"
 * Copyright 2013-2014  R. Lemos
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * END COPYRIGHT NOTICE
 ******************************************************************************/
#include <math.h>
#include <alsa/asoundlib.h>

static void
error(const char *fmt,...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "webmixer: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
}

static struct snd_mixer_selem_regopt
smixer_options = {
	.ver = 1,
	.abstract = SND_MIXER_SABSTRACT_NONE,
};

struct volume_ops {
	int (*get_range)(snd_mixer_elem_t *elem, long *min, long *max);
	int (*get)(snd_mixer_elem_t *elem, snd_mixer_selem_channel_id_t c,
		   long *value);
};
	
enum { VOL_RAW, VOL_DB };

struct volume_ops_set {
	int (*has_volume)(snd_mixer_elem_t *elem);
	struct volume_ops v[2];
};

static const struct volume_ops_set
vol_ops[2] = {
	{
		.has_volume = snd_mixer_selem_has_playback_volume,
		.v = {{ snd_mixer_selem_get_playback_volume_range,
			snd_mixer_selem_get_playback_volume },
		      { snd_mixer_selem_get_playback_dB_range,
			snd_mixer_selem_get_playback_dB },
		},
	},
	{
		.has_volume = snd_mixer_selem_has_capture_volume,
		.v = {{ snd_mixer_selem_get_capture_volume_range,
			snd_mixer_selem_get_capture_volume },
		      { snd_mixer_selem_get_capture_dB_range,
			snd_mixer_selem_get_capture_dB },
		},
	},
};

static int
convert_prange(long val, long min, long max)
{
	long range = max - min;
	int tmp;

	if (range == 0)
		return 0;
	val -= min;
	tmp = rint((double)val/(double)range * 100);
	return tmp;
}

static void
print_dB(long dB)
{
	if (dB < 0) {
		printf("-%li.%02lidB", -dB / 100, -dB % 100);
	} else {
		printf("%li.%02lidB", dB / 100, dB % 100);
	}
}

static void
show_selem_volume(snd_mixer_elem_t *elem, 
		  snd_mixer_selem_channel_id_t chn, int dir,
		  long min, long max)
{
	long raw, val;
	vol_ops[dir].v[VOL_RAW].get(elem, chn, &raw);
	val = convert_prange(raw, min, max);
	printf(" %li [%li%%]", raw, val);
	if (!vol_ops[dir].v[VOL_DB].get(elem, chn, &val)) {
		printf(" [");
		print_dB(val);
		printf("]");
	}
}

static int 
show_selem(snd_mixer_t *handle, snd_mixer_selem_id_t *id, const char *space, const char *card)
{
	snd_mixer_selem_channel_id_t chn;
	long pmin = 0, pmax = 0;
	long cmin = 0, cmax = 0;
	int psw, csw;
	int pmono, cmono, mono_ok = 0;
	snd_mixer_elem_t *elem;
	
	elem = snd_mixer_find_selem(handle, id);
	if (!elem) {
		error("Mixer %s simple element not found", card);
		return -ENOENT;
	}

	printf("%sCapabilities:", space);
	if (snd_mixer_selem_has_common_volume(elem)) {
		printf(" volume");
		if (snd_mixer_selem_has_playback_volume_joined(elem))
			printf(" volume-joined");
	} else {
		if (snd_mixer_selem_has_playback_volume(elem)) {
			printf(" pvolume");
			if (snd_mixer_selem_has_playback_volume_joined(elem))
				printf(" pvolume-joined");
		}
		if (snd_mixer_selem_has_capture_volume(elem)) {
			printf(" cvolume");
			if (snd_mixer_selem_has_capture_volume_joined(elem))
				printf(" cvolume-joined");
		}
	}
	if (snd_mixer_selem_has_common_switch(elem)) {
		printf(" switch");
		if (snd_mixer_selem_has_playback_switch_joined(elem))
			printf(" switch-joined");
	} else {
		if (snd_mixer_selem_has_playback_switch(elem)) {
			printf(" pswitch");
			if (snd_mixer_selem_has_playback_switch_joined(elem))
				printf(" pswitch-joined");
		}
		if (snd_mixer_selem_has_capture_switch(elem)) {
			printf(" cswitch");
			if (snd_mixer_selem_has_capture_switch_joined(elem))
				printf(" cswitch-joined");
			if (snd_mixer_selem_has_capture_switch_exclusive(elem))
				printf(" cswitch-exclusive");
		}
	}
	if (snd_mixer_selem_is_enum_playback(elem)) {
		printf(" penum");
	} else if (snd_mixer_selem_is_enum_capture(elem)) {
		printf(" cenum");
	} else if (snd_mixer_selem_is_enumerated(elem)) {
		printf(" enum");
	}
	printf("\n");
	if (snd_mixer_selem_is_enumerated(elem)) {
		int i, items;
		unsigned int idx;
		char itemname[40];
		items = snd_mixer_selem_get_enum_items(elem);
		printf("  Items:");
		for (i = 0; i < items; i++) {
			snd_mixer_selem_get_enum_item_name(elem, i, sizeof(itemname) - 1, itemname);
			printf(" '%s'", itemname);
		}
		printf("\n");
		for (i = 0; !snd_mixer_selem_get_enum_item(elem, i, &idx); i++) {
			snd_mixer_selem_get_enum_item_name(elem, idx, sizeof(itemname) - 1, itemname);
			printf("  Item%d: '%s'\n", i, itemname);
		}
		return 0; /* no more thing to do */
	}
	if (snd_mixer_selem_has_capture_switch_exclusive(elem))
		printf("%sCapture exclusive group: %i\n", space,
		       snd_mixer_selem_get_capture_group(elem));
	if (snd_mixer_selem_has_playback_volume(elem) ||
	    snd_mixer_selem_has_playback_switch(elem)) {
		printf("%sPlayback channels:", space);
		if (snd_mixer_selem_is_playback_mono(elem)) {
			printf(" Mono");
		} else {
			int first = 1;
			for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++){
				if (!snd_mixer_selem_has_playback_channel(elem, chn))
					continue;
				if (!first)
					printf(" -");
				printf(" %s", snd_mixer_selem_channel_name(chn));
				first = 0;
			}
		}
		printf("\n");
	}
	if (snd_mixer_selem_has_capture_volume(elem) ||
	    snd_mixer_selem_has_capture_switch(elem)) {
		printf("%sCapture channels:", space);
		if (snd_mixer_selem_is_capture_mono(elem)) {
			printf(" Mono");
		} else {
			int first = 1;
			for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++){
				if (!snd_mixer_selem_has_capture_channel(elem, chn))
					continue;
				if (!first)
					printf(" -");
				printf(" %s", snd_mixer_selem_channel_name(chn));
				first = 0;
			}
		}
		printf("\n");
	}
	if (snd_mixer_selem_has_playback_volume(elem) ||
	    snd_mixer_selem_has_capture_volume(elem)) {
		printf("%sLimits:", space);
		if (snd_mixer_selem_has_common_volume(elem)) {
			snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
			snd_mixer_selem_get_capture_volume_range(elem, &cmin, &cmax);
			printf(" %li - %li", pmin, pmax);
		} else {
			if (snd_mixer_selem_has_playback_volume(elem)) {
				snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
				printf(" Playback %li - %li", pmin, pmax);
			}
			if (snd_mixer_selem_has_capture_volume(elem)) {
				snd_mixer_selem_get_capture_volume_range(elem, &cmin, &cmax);
				printf(" Capture %li - %li", cmin, cmax);
			}
		}
		printf("\n");
	}
	pmono = snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_MONO) &&
	        (snd_mixer_selem_is_playback_mono(elem) || 
		 (!snd_mixer_selem_has_playback_volume(elem) &&
		  !snd_mixer_selem_has_playback_switch(elem)));
	cmono = snd_mixer_selem_has_capture_channel(elem, SND_MIXER_SCHN_MONO) &&
	        (snd_mixer_selem_is_capture_mono(elem) || 
		 (!snd_mixer_selem_has_capture_volume(elem) &&
		  !snd_mixer_selem_has_capture_switch(elem)));
#if 0
	printf("pmono = %i, cmono = %i (%i, %i, %i, %i)\n", pmono, cmono,
			snd_mixer_selem_has_capture_channel(elem, SND_MIXER_SCHN_MONO),
			snd_mixer_selem_is_capture_mono(elem),
			snd_mixer_selem_has_capture_volume(elem),
			snd_mixer_selem_has_capture_switch(elem));
#endif
	if (pmono || cmono) {
		if (!mono_ok) {
			printf("%s%s:", space, "Mono");
			mono_ok = 1;
		}
		if (snd_mixer_selem_has_common_volume(elem)) {
			show_selem_volume(elem, SND_MIXER_SCHN_MONO, 0, pmin, pmax);
		}
		if (snd_mixer_selem_has_common_switch(elem)) {
			snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &psw);
			printf(" [%s]", psw ? "on" : "off");
		}
	}
	if (pmono && snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_MONO)) {
		int title = 0;
		if (!mono_ok) {
			printf("%s%s:", space, "Mono");
			mono_ok = 1;
		}
		if (!snd_mixer_selem_has_common_volume(elem)) {
			if (snd_mixer_selem_has_playback_volume(elem)) {
				printf(" Playback");
				title = 1;
				show_selem_volume(elem, SND_MIXER_SCHN_MONO, 0, pmin, pmax);
			}
		}
		if (!snd_mixer_selem_has_common_switch(elem)) {
			if (snd_mixer_selem_has_playback_switch(elem)) {
				if (!title)
					printf(" Playback");
				snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &psw);
				printf(" [%s]", psw ? "on" : "off");
			}
		}
	}
	if (cmono && snd_mixer_selem_has_capture_channel(elem, SND_MIXER_SCHN_MONO)) {
		int title = 0;
		if (!mono_ok) {
			printf("%s%s:", space, "Mono");
			mono_ok = 1;
		}
		if (!snd_mixer_selem_has_common_volume(elem)) {
			if (snd_mixer_selem_has_capture_volume(elem)) {
				printf(" Capture");
				title = 1;
				show_selem_volume(elem, SND_MIXER_SCHN_MONO, 1, cmin, cmax);
			}
		}
		if (!snd_mixer_selem_has_common_switch(elem)) {
			if (snd_mixer_selem_has_capture_switch(elem)) {
				if (!title)
					printf(" Capture");
				snd_mixer_selem_get_capture_switch(elem, SND_MIXER_SCHN_MONO, &csw);
				printf(" [%s]", csw ? "on" : "off");
			}
		}
	}
	if (pmono || cmono)
		printf("\n");
	if (!pmono || !cmono) {
		for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
			if ((pmono || !snd_mixer_selem_has_playback_channel(elem, chn)) &&
			    (cmono || !snd_mixer_selem_has_capture_channel(elem, chn)))
				continue;
			printf("%s%s:", space, snd_mixer_selem_channel_name(chn));
			if (!pmono && !cmono && snd_mixer_selem_has_common_volume(elem)) {
				show_selem_volume(elem, chn, 0, pmin, pmax);
			}
			if (!pmono && !cmono && snd_mixer_selem_has_common_switch(elem)) {
				snd_mixer_selem_get_playback_switch(elem, chn, &psw);
				printf(" [%s]", psw ? "on" : "off");
			}
			if (!pmono && snd_mixer_selem_has_playback_channel(elem, chn)) {
				int title = 0;
				if (!snd_mixer_selem_has_common_volume(elem)) {
					if (snd_mixer_selem_has_playback_volume(elem)) {
						printf(" Playback");
						title = 1;
						show_selem_volume(elem, chn, 0, pmin, pmax);
					}
				}
				if (!snd_mixer_selem_has_common_switch(elem)) {
					if (snd_mixer_selem_has_playback_switch(elem)) {
						if (!title)
							printf(" Playback");
						snd_mixer_selem_get_playback_switch(elem, chn, &psw);
						printf(" [%s]", psw ? "on" : "off");
					}
				}
			}
			if (!cmono && snd_mixer_selem_has_capture_channel(elem, chn)) {
				int title = 0;
				if (!snd_mixer_selem_has_common_volume(elem)) {
					if (snd_mixer_selem_has_capture_volume(elem)) {
						printf(" Capture");
						title = 1;
						show_selem_volume(elem, chn, 1, cmin, cmax);
					}
				}
				if (!snd_mixer_selem_has_common_switch(elem)) {
					if (snd_mixer_selem_has_capture_switch(elem)) {
						if (!title)
							printf(" Capture");
						snd_mixer_selem_get_capture_switch(elem, chn, &csw);
						printf(" [%s]", csw ? "on" : "off");
					}
				}
			}
			printf("\n");
		}
	}
	return 0;
}

int
show_card_mixer (const char *card)
{
	int err;
	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_alloca(&sid);
	
	if ((err = snd_mixer_open(&handle, 0)) < 0) {
		error("Mixer %s open error: %s", card, snd_strerror(err));
		return err;
	}

	smixer_options.device = card;

	if ((err = snd_mixer_selem_register(handle, &smixer_options, NULL)) < 0) {
		error("Mixer register error: %s", snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	err = snd_mixer_load(handle);
	if (err < 0) {
		error("Mixer %s load error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
		snd_mixer_selem_get_id(elem, sid);
		if (!snd_mixer_selem_is_active(elem))
			printf("[INACTIVE] ");
		printf("Simple mixer control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
		show_selem(handle, sid, "  ", card);
	}
	snd_mixer_close(handle);

	return 0;
}

int
show_card (const char *card)
{
	int err;

	snd_ctl_t *handle;
	snd_ctl_card_info_t *info;
	snd_ctl_card_info_alloca(&info);

	if ((err = snd_ctl_open(&handle, card, 0)) < 0) {
		error("control open (%s): %s", card, snd_strerror(err));
		return err;
	}
	if ((err = snd_ctl_card_info(handle, info)) < 0) {
		error("control hardware info (%s): %s", card, snd_strerror(err));
		snd_ctl_close(handle);
		return err;
	}

	printf("card\n");
	printf("get_id: %s\n", snd_ctl_card_info_get_id(info));
	printf("get_driver: %s\n", snd_ctl_card_info_get_driver(info));
	printf("get_name: %s\n", snd_ctl_card_info_get_name(info));
	printf("get_longname: %s\n", snd_ctl_card_info_get_longname(info));
	printf("get_mixername: %s\n", snd_ctl_card_info_get_mixername(info));
	printf("get_components: %s\n", snd_ctl_card_info_get_components(info));

	printf("card %s: %s [%s]\n",
		card, snd_ctl_card_info_get_id(info), snd_ctl_card_info_get_name(info));

	snd_ctl_close(handle);

	if ((err = show_card_mixer(card)) < 0) {
		error("show_card_mixer");
		return err;
	}

	return 0;
}

int
main (int argc, char *argv[])
{
	int err;
	int card;

	card = -1;
	if ((err = snd_card_next(&card)) < 0 || card < 0) {
		error("no soundcards found...");
		return err;
	}

	while (card >= 0) {
		char name[32];
		sprintf(name, "hw:%d", card);

		if ((err = show_card(name)) < 0) {
			error("show_card");
			return err;
		}

		if ((err = snd_card_next(&card)) < 0) {
			error("snd_card_next");
			return err;
		}
	}

	return 0;
}
