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
#include <stdio.h>
#include <string.h>
#include <jansson.h>

int
main (int argc, char *argv[])
{
	json_t *cards = json_loadf(stdin, 0, NULL);
	
	json_t *card;
	size_t i;

	json_array_foreach(cards, i, card) {
		json_t *selems = json_object_get(card, "mixer");

		json_t *selem;
		size_t j;

		printf("card\n");
		printf("get_id: %s\n", json_string_value(json_object_get(card, "id")));
		printf("get_driver: %s\n", json_string_value(json_object_get(card, "driver")));
		printf("get_name: %s\n", json_string_value(json_object_get(card, "name")));
		printf("get_longname: %s\n", json_string_value(json_object_get(card, "longname")));
		printf("get_mixername: %s\n", json_string_value(json_object_get(card, "mixername")));
		printf("get_components: %s\n", json_string_value(json_object_get(card, "components")));

		printf("card %s: %s [%s]\n",
			"**LOST**", json_string_value(json_object_get(card, "id")), json_string_value(json_object_get(card, "name")));
		
		json_array_foreach(selems, j, selem) {
			json_t *inactive = json_object_get(selem, "inactive");
			json_t *values = json_object_get(selem, "value");

			int is_enum = 0;

			if (inactive == json_true())
				printf("[INACTIVE] ");

			printf("Simple mixer control '%s',%i\n", json_string_value(json_object_get(selem, "name")), (int)json_integer_value(json_object_get(selem, "index")));

			{
				json_t *capabilities = json_object_get(selem, "capabilities");

				json_t *capability;
				size_t k;

				printf("  Capabilities:");
				json_array_foreach(capabilities, k, capability) {
					const char *cap = json_string_value(capability);
					is_enum |= !strcmp("enum", cap) || !strcmp("penum", cap) || !strcmp("cenum", cap);
					printf(" %s", cap);
				}
				printf("\n");
			}

			if (is_enum) {
				json_t *alternatives = json_object_get(selem, "alternatives");

				json_t *value;
				size_t k;
				
				printf("  Items:");
				json_array_foreach(alternatives, k, value) {
					printf(" '%s'", json_string_value(value));
				}
				printf("\n");

				json_array_foreach(values, k, value) {
					printf("  Item%zu: '%s'\n", k, json_string_value(value));
				}
			} else {
				json_t *captureExclusiveGroup = json_object_get(selem, "captureExclusiveGroup");
				json_t *playbackChannels = json_object_get(selem, "playbackChannels");
				json_t *captureChannels = json_object_get(selem, "captureChannels");
				json_t *limits = json_object_get(selem, "limits");

				json_t *value;
				size_t k;

				if (captureExclusiveGroup)
					printf("  Capture exclusive group: %" JSON_INTEGER_FORMAT "\n", json_integer_value(captureExclusiveGroup));

				if (playbackChannels) {
					char *sep = "";
					json_t *playbackChannel;

					printf("  Playback channels:");
					json_array_foreach(playbackChannels, k, playbackChannel) {
						printf("%s %s", sep, json_string_value(playbackChannel));
						sep = " -";
					}
					printf("\n");
				}
			
				if (captureChannels) {
					char *sep = "";
					json_t *captureChannel;

					printf("  Capture channels:");
					json_array_foreach(captureChannels, k, captureChannel) {
						printf("%s %s", sep, json_string_value(captureChannel));
						sep = " -";
					}
					printf("\n");
				}

				if (limits) {
					json_t *common = json_object_get(limits, "common");

					printf("  Limits:");
					if (common) {
						printf(" %" JSON_INTEGER_FORMAT " - %" JSON_INTEGER_FORMAT, json_integer_value(json_object_get(common, "min")), json_integer_value(json_object_get(common, "max")));
					} else {
						json_t *playback = json_object_get(limits, "playback");
						json_t *capture = json_object_get(limits, "capture");

						if (playback)
							printf(" Playback %" JSON_INTEGER_FORMAT " - %" JSON_INTEGER_FORMAT, json_integer_value(json_object_get(playback, "min")), json_integer_value(json_object_get(playback, "max")));
						if (capture)
							printf(" Capture %" JSON_INTEGER_FORMAT " - %" JSON_INTEGER_FORMAT, json_integer_value(json_object_get(capture, "min")), json_integer_value(json_object_get(capture, "max")));
					}
					printf("\n");
				}


				json_array_foreach(values, k, value) {
					json_t *volume = json_object_get(value, "volume");
					json_t *playback = json_object_get(value, "playback");
					json_t *capture = json_object_get(value, "capture");

					printf("  %s:", json_string_value(json_object_get(value, "channel")));


					if (volume) {
						json_t *db = json_object_get(volume, "dB");

						printf(" %" JSON_INTEGER_FORMAT " [%" JSON_INTEGER_FORMAT "%%]", json_integer_value(json_object_get(volume, "raw")), json_integer_value(json_object_get(volume, "perc")));
						if (db) {
							json_int_t dbv = json_integer_value(db);
							if (dbv < 0) 
								printf(" [-%" JSON_INTEGER_FORMAT ".%02" JSON_INTEGER_FORMAT "dB]", -dbv/100, -dbv % 100);
							else
								printf(" [%" JSON_INTEGER_FORMAT ".%02" JSON_INTEGER_FORMAT "dB]", dbv/100, dbv % 100);
						}
					} 

					if (playback) {
						json_t *volume = json_object_get(playback, "volume");
						json_t *sw = json_object_get(playback, "switch");

						if (volume || sw)
							printf(" Playback");
	
						if (volume) {
							json_t *db = json_object_get(volume, "dB");

							printf(" %" JSON_INTEGER_FORMAT " [%" JSON_INTEGER_FORMAT "%%]", json_integer_value(json_object_get(volume, "raw")), json_integer_value(json_object_get(volume, "perc")));
							if (db) {
								json_int_t dbv = json_integer_value(db);
								if (dbv < 0) 
									printf(" [-%" JSON_INTEGER_FORMAT ".%02" JSON_INTEGER_FORMAT "dB]", -dbv/100, -dbv % 100);
								else
									printf(" [%" JSON_INTEGER_FORMAT ".%02" JSON_INTEGER_FORMAT "dB]", dbv/100, dbv % 100);
							}
						}

						if (sw) 
							printf(" [%s]", json_string_value(sw));
					}

					if (capture) {
						json_t *volume = json_object_get(capture, "volume");
						json_t *sw = json_object_get(capture, "switch");

						if (volume || sw)
							printf(" Capture");
	
						if (volume) {
							json_t *db = json_object_get(volume, "dB");

							printf(" %" JSON_INTEGER_FORMAT " [%" JSON_INTEGER_FORMAT "%%]", json_integer_value(json_object_get(volume, "raw")), json_integer_value(json_object_get(volume, "perc")));
							if (db) {
								json_int_t dbv = json_integer_value(db);
								if (dbv < 0) 
									printf(" [-%" JSON_INTEGER_FORMAT ".%02" JSON_INTEGER_FORMAT "dB]", -dbv/100, -dbv % 100);
								else
									printf(" [%" JSON_INTEGER_FORMAT ".%02" JSON_INTEGER_FORMAT "dB]", dbv/100, dbv % 100);
							}
						}

						if (sw) 
							printf(" [%s]", json_string_value(sw));
					}
					printf("\n");
				}
			}
		}
	}

	json_decref(cards);

	return 0;
}
