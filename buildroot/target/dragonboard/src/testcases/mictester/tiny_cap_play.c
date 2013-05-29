/* tinycap.c
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

#include "include/tinyalsa/asoundlib.h"

#include "dragonboard_inc.h"

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

struct wav_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t riff_fmt;
    uint32_t fmt_id;
    uint32_t fmt_sz;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_id;
    uint32_t data_sz;
};

int capturing = 1;

unsigned int cap_play_sample(unsigned int device,
                            unsigned int channels, unsigned int rate,
                            unsigned int bits);

static void tinymix_set_value(struct mixer *mixer, unsigned int id,
                              unsigned int value);

void sigint_handler(int sig)
{
    capturing = 0;
}

int main(int argc, char **argv)
{
    struct wav_header header;
    struct mixer *mixer;

    //unsigned int device = 0;
    unsigned int channels = 2;
    unsigned int rate = 44100;
    unsigned int bits = 16;
    //unsigned int frames;
    int delay;

    INIT_CMD_PIPE();
    init_script(atoi(argv[2]));

    /* default: delay 5 second to go on */
    delay = 0;
    if (script_fetch("mic", "delay", &delay, 1) || 
            delay < 0) {
        sleep(5);
    }
    else if (delay > 0) {
        sleep(delay);
    }

    header.riff_id = ID_RIFF;
    header.riff_sz = 0;
    header.riff_fmt = ID_WAVE;
    header.fmt_id = ID_FMT;
    header.fmt_sz = 16;
    header.audio_format = FORMAT_PCM;
    header.num_channels = channels;
    header.sample_rate = rate;
    header.byte_rate = (header.bits_per_sample / 8) * channels * rate;
    header.block_align = channels * (header.bits_per_sample / 8);
    header.bits_per_sample = bits;
    header.data_id = ID_DATA;

    mixer = mixer_open(0);
    if (!mixer) {
        fprintf(stderr, "Failed to open mixer\n");
        goto out;
    }
	
	tinymix_set_value(mixer, 1, 1);//pamut Enable
	tinymix_set_value(mixer, 2, 1);//Mixpas Enable   	
	tinymix_set_value(mixer, 4, 12);//Mic1RS Enable               
    //tinymix_set_value(mixer, 4, 3);//Mic2RS Enable               
	tinymix_set_value(mixer, 29, 1);//Mic1 amplifier enable
    //tinymix_set_value(mixer, 28, 1);//Mic2 amplifier enable
	tinymix_set_value(mixer, 27, 1);//VMic enable
	tinymix_set_value(mixer, 15, 1);//Mix enable
    /* install signal handler and begin capturing */
    signal(SIGINT, sigint_handler);
	
    SEND_CMD_PIPE_OK();

	while (capturing) {
	    sleep(600);
	}
	
	tinymix_set_value(mixer, 1, 0);//pamut Enable
	tinymix_set_value(mixer, 2, 0);//Mixpas Enable
    tinymix_set_value(mixer, 4, 0);//Mic1RS Enable
	//tinymix_set_value(mixer, 12, 1);//Mic2RS Enable
	tinymix_set_value(mixer, 29, 0);//Mic1 amplifier enable
	tinymix_set_value(mixer, 27, 0);//VMic enable
	tinymix_set_value(mixer, 15, 0);//Mix enable

out:
    deinit_script();
	EXIT_CMD_PIPE();

    return 0;
}

unsigned int cap_play_sample(unsigned int device,
                            unsigned int channels, unsigned int rate,
                            unsigned int bits)
{
    struct pcm_config cap_config;
    struct pcm_config play_config;
    struct pcm *cap_pcm;
    struct pcm *play_pcm;
	
    char *buffer;
    unsigned int size;
    //unsigned int bytes_read = 0;

    cap_config.channels = channels;
    cap_config.rate = rate;
    cap_config.period_size = 1024;
    cap_config.period_count = 4;
    if (bits == 32) {
        cap_config.format = PCM_FORMAT_S32_LE;
    } else if (bits == 16) {
        cap_config.format = PCM_FORMAT_S16_LE;
    }
    cap_config.start_threshold = 0;
    cap_config.stop_threshold = 0;
    cap_config.silence_threshold = 0;

    play_config.channels = channels;
    play_config.rate = rate;
    play_config.period_size = 1024;
    play_config.period_count = 4;
    if (bits == 32)
        play_config.format = PCM_FORMAT_S32_LE;
    else if (bits == 16)
        play_config.format = PCM_FORMAT_S16_LE;
    play_config.start_threshold = 0;
    play_config.stop_threshold = 0;
    play_config.silence_threshold = 0;


    cap_pcm = pcm_open(0, device, PCM_IN, &cap_config);
    play_pcm = pcm_open(0, device, PCM_OUT, &play_config);
    if (!cap_pcm || !pcm_is_ready(cap_pcm) || !play_pcm || !pcm_is_ready(play_pcm)) {
        fprintf(stderr, "Unable to open PCM device (%s)\n",
                pcm_get_error(cap_pcm));
        return 0;
    }

    size = pcm_get_buffer_size(cap_pcm);
    buffer = malloc(size);    
    if (!buffer) {
        fprintf(stderr, "Unable to allocate %d bytes\n", size);
        free(buffer);
        pcm_close(cap_pcm);
        pcm_close(play_pcm);
        return 0;
    }

    printf("Capturing sample: %u ch, %u hz, %u bit\n", channels, rate, bits);

    while (capturing ) {
    	int ret = 0;
    	ret = pcm_read(cap_pcm, buffer, size);
    	
    	if (ret == 0) {
    		if (pcm_write(play_pcm, buffer, size)) {
    		    fprintf(stderr, "Error playing sample\n");
                break;
    		}
    	}
    }

    free(buffer);
    pcm_close(cap_pcm);
    pcm_close(play_pcm);    

    return 0;
}

static void tinymix_set_value(struct mixer *mixer, unsigned int id,
                              unsigned int value)
{
    struct mixer_ctl *ctl;
    enum mixer_ctl_type type;
    unsigned int num_values;
    unsigned int i;

    ctl = mixer_get_ctl(mixer, id);
    type = mixer_ctl_get_type(ctl);
    num_values = mixer_ctl_get_num_values(ctl);

    for (i = 0; i < num_values; i++) {
        if (mixer_ctl_set_value(ctl, i, value)) {
            fprintf(stderr, "Error: invalid value\n");
            return;
        }
    }
}
