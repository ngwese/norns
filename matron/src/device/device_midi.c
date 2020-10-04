#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdio.h>

#include "../events.h"

#include "../clocks/clock_midi.h"
#include "device.h"
#include "device_midi.h"

#define DEV_MIDI_RX_BUFFER_SIZE 512

unsigned int dev_midi_port_count(const char *path) {
    int card;
    int alsa_dev;

    if (sscanf(path, "/dev/snd/midiC%dD%d", &card, &alsa_dev) < 0) {
        // TODO: Insert error message here
        return 0;
    }

    // mostly from amidi.c
    snd_ctl_t *ctl;
    char name[32];

    snd_rawmidi_info_t *info;
    int subs = 0;
    int subs_in = 0;
    int subs_out = 0;

    sprintf(name, "hw:%d", card);
    if (snd_ctl_open(&ctl, name, 0) < 0) {
        // TODO: Insert error message here
        return 0;
    }

    snd_rawmidi_info_alloca(&info);
    snd_rawmidi_info_set_device(info, alsa_dev);

    snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
    if (snd_ctl_rawmidi_info(ctl, info) >= 0) {
        subs_in = snd_rawmidi_info_get_subdevices_count(info);
    }

    snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
    if (snd_ctl_rawmidi_info(ctl, info) >= 0) {
        subs_out = snd_rawmidi_info_get_subdevices_count(info);
    }
    snd_ctl_close(ctl);

    subs = subs_in > subs_out ? subs_in : subs_out;
    return subs;
}

int dev_midi_init(void *self, unsigned int port_index, bool multiport_device) {
    struct dev_midi *midi = (struct dev_midi *)self;
    struct dev_common *base = (struct dev_common *)self;

    unsigned int alsa_card;
    unsigned int alsa_dev;
    char *alsa_name;

    snd_rawmidi_params_t *params;

    sscanf(base->path, "/dev/snd/midiC%uD%u", &alsa_card, &alsa_dev);

    if (asprintf(&alsa_name, "hw:%u,%u,%u", alsa_card, alsa_dev, port_index) < 0) {
        fprintf(stderr, "failed to create alsa device name for card %d,%d\n", alsa_card, alsa_dev);
        return -1;
    }

    if (snd_rawmidi_open(&midi->handle_in, &midi->handle_out, alsa_name, 0) < 0) {
        fprintf(stderr, "failed to open alsa device %s\n", alsa_name);
        return -1;
    }

    snd_rawmidi_params_malloc(&params);
    snd_rawmidi_params_current(midi->handle_in, params);
    fprintf(stderr, "rawmidi params for: %s\n", base->name);
    fprintf(stderr, "  avail_min: %u\n", snd_rawmidi_params_get_avail_min(params));
    fprintf(stderr, "  buff_size: %u\n", snd_rawmidi_params_get_buffer_size(params));
    fprintf(stderr, " no_sensing: %u\n", snd_rawmidi_params_get_no_active_sensing(params));
    snd_rawmidi_params_free(params);

    char *name_with_port_index;
    if (multiport_device) {
        if (asprintf(&name_with_port_index, "%s %u", base->name, port_index + 1) < 0) {
            fprintf(stderr, "failed to create human-readable device name for card %d,%d,%d\n", alsa_card, alsa_dev,
                    port_index);
            return -1;
        }
        base->name = name_with_port_index;
    }

    base->start = &dev_midi_start;
    base->deinit = &dev_midi_deinit;

    return 0;
}

void dev_midi_deinit(void *self) {
    struct dev_midi *midi = (struct dev_midi *)self;
    snd_rawmidi_close(midi->handle_in);
    snd_rawmidi_close(midi->handle_out);
}

inline ssize_t dev_midi_post_events(uint8_t *rx_buffer, ssize_t size, uint8_t *status, uint8_t *data_len, struct dev_midi *midi) {
    union event_data *ev;

    uint8_t *byte = rx_buffer;
    uint8_t *end = rx_buffer + size;

    // uint8_t status = 0;
    // uint8_t data_len = 0;
    uint8_t i;

    fprintf(stderr, "buf 0x%p, read %zd begin normalize // head 0x%02x\n", rx_buffer, size, *byte);

    // normalize events, handling running status
    while (byte < end) {
        // system real time
        // if ((*byte >= 0xf8) && (*byte <= 0xff)) {
        if (*byte >= 0xf8) {
            clock_midi_handle_message(*byte++);
            *data_len = 0;
            goto post;
        }

        // channel voice messages
        switch (*byte & 0xf0) {
        case 0x80:
        case 0x90:
        case 0xa0:
        case 0xb0:
        case 0xe0:
            *status = *byte++;
            *data_len = 2;
            goto post;
        case 0xc0:
        case 0xd0:
            *status = *byte++;
            *data_len = 1;
            goto post;
        }

        // channel mode messages
        if (*byte >= 0x79 && *byte <= 0x7f) {
            *status = *byte++;
            *data_len = 1;
            goto post;
        }

        // system common messages
        switch (*byte) {
        case 0xf2:
            *status = *byte++;
            *data_len = 2;
            goto post;
        case 0xf1:
        case 0xf3:
            *status = *byte++;
            *data_len = 1;
            goto post;
        case 0xf6:
            *status = *byte++;
            *data_len = 0;
            goto post;
        }

        // system exclusive, potentially continued from previous rx buffer
        if (*byte == 0xf7 || *status == 0xf7) {
            fprintf(stderr, "eating sysx\n");
            *status = *byte;
            *data_len = 0;
            // consume and discard
            while (*byte != 0xf0 && byte <= end) { ++byte; };
            continue;
        }

        // no status byte, running status applies, prior status and data len
        // applies
        fprintf(stderr, "RMS? byte: 0x%02x -- last s 0x%x n %u\n", *byte, *status, *data_len);
        if (*status == 0) {
            ++byte;
            fprintf(stderr, "corrupt?, not posting");
            continue;
        }

    post:
        ev = event_data_new(EVENT_MIDI_EVENT);
        ev->midi_event.id = midi->dev.id;
        ev->midi_event.nbytes = *data_len + 1;
        ev->midi_event.data[0] = *status;
        for (i = 1; i <= *data_len; i++) {
            ev->midi_event.data[i] = *byte++;
        }
        fprintf(stderr, "POST(%d // 0x%x 0x%x 0x%x)\n", *data_len + 1,
            ev->midi_event.data[0],
            ev->midi_event.data[1],
            ev->midi_event.data[2]
        );
        event_post(ev);
    }

    ssize_t rtn = byte - rx_buffer;
    fprintf(stderr, "buf 0x%p, read %zd, rtn %zd\n", rx_buffer, size, rtn);
    return rtn;
}

void *dev_midi_start(void *self) {
    struct dev_midi *midi = (struct dev_midi *)self;
    struct dev_common *base = (struct dev_common *)self;

    snd_rawmidi_status_t *status = NULL;
    ssize_t xruns;

    ssize_t read = 0;
    uint8_t rx_buffer[DEV_MIDI_RX_BUFFER_SIZE];
    uint8_t data_status = 0;
    uint8_t data_len = 0;

    if (snd_rawmidi_status_malloc(&status) != 0) {
        fprintf(stderr, "failed allocating rawmidi status, stopping device: %s\n", base->name);
        return NULL;
    }

    do {
        read = snd_rawmidi_read(midi->handle_in, rx_buffer, DEV_MIDI_RX_BUFFER_SIZE);
        if (dev_midi_post_events(rx_buffer, read, &data_status, &data_len, midi) != read) {
            fprintf(stderr, "midi event post inconsistency for device: %s\n", base->name);
        }

        // TODO: verify this is not overly expensive to do
        if (snd_rawmidi_status(midi->handle_in, status) == 0) {
            xruns = snd_rawmidi_status_get_xruns(status);
            if (xruns > 0) {
                fprintf(stderr, "xruns (%d) for midi device: %s\n", xruns, base->name);
                // TODO: validate that flushing the ring buffer gets us back in a good state
                snd_rawmidi_drop(midi->handle_in);
                status = 0;
                data_len = 0;
            }
        }

    } while (read > 0);

    if (status) {
        snd_rawmidi_status_free(status);
    }

    return NULL;
}

void *dev_midi_start_old(void *self) {
    struct dev_midi *midi = (struct dev_midi *)self;
    union event_data *ev;

    ssize_t read = 0;
    uint8_t byte = 0;
    uint8_t msg_buf[256];
    uint8_t msg_pos = 0;
    uint8_t msg_len = 0;

    do {
        read = snd_rawmidi_read(midi->handle_in, &byte, 1);

        if (byte >= 0xf8) {
            clock_midi_handle_message(byte);

            ev = event_data_new(EVENT_MIDI_EVENT);
            ev->midi_event.id = midi->dev.id;
            ev->midi_event.data[0] = byte;
            ev->midi_event.nbytes = 1;
            event_post(ev);
        } else {
            if (byte >= 0x80) {
                msg_buf[0] = byte;
                msg_pos = 1;

                switch (byte & 0xf0) {
                case 0x80:
                case 0x90:
                case 0xa0:
                case 0xb0:
                case 0xe0:
                case 0xf2:
                    msg_len = 3;
                    break;
                case 0xc0:
                case 0xd0:
                    msg_len = 2;
                    break;
                case 0xf0:
                    switch (byte & 0x0f) {
                    case 0x01:
                    case 0x03:
                        msg_len = 2;
                        break;
                    case 0x07:
                        // TODO: properly handle sysex length
                        msg_len = msg_pos; // sysex end
                        break;
                    case 0x00:
                        msg_len = 0; // sysex start
                        break;
                    default:
                        msg_len = 2;
                        break;
                    }
                    break;
                default:
                    msg_len = 2;
                    break;
                }
            } else {
                msg_buf[msg_pos] = byte;
                msg_pos += 1;
            }

            if (msg_pos == msg_len) {
                ev = event_data_new(EVENT_MIDI_EVENT);
                ev->midi_event.id = midi->dev.id;
                ev->midi_event.data[0] = msg_buf[0];
                ev->midi_event.data[1] = msg_len > 1 ? msg_buf[1] : 0;
                ev->midi_event.data[2] = msg_len > 2 ? msg_buf[2] : 0;
                ev->midi_event.nbytes = msg_len;
                event_post(ev);

                msg_pos = 0;
                msg_len = 0;
            }
        }
    } while (read > 0);

    return NULL;
}

ssize_t dev_midi_send(void *self, uint8_t *data, size_t n) {
    struct dev_midi *midi = (struct dev_midi *)self;
    return snd_rawmidi_write(midi->handle_out, data, n);
}
