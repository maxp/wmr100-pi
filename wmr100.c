/**
 * Oregon Scientific WMR100/200 protocol. Tested on wmr100.
 *
 * Copyright 2009 Barnaby Gray <barnaby@pickle.me.uk>
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
 */

/*
    completely refactored by m@penzin.ru
*/

#include <hid.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

#define WMR100_VENDOR_ID  0x0fde
#define WMR100_PRODUCT_ID 0xca01

#define MAXSENSORS 5
#define RECORD_HISTORY 60

#define RECV_PACKET_LEN 8
#define BUFF_SIZE 255

unsigned char const PATHLEN = 2;
int const PATH_IN[]  = { 0xff000001, 0xff000001 };
int const PATH_OUT[] = { 0xff000001, 0xff000002 };

unsigned char const INIT_PACKET1[] = { 0x20, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00 };
unsigned char const INIT_PACKET2[] = { 0x01, 0xd0, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00 };

typedef struct _WMR {
    int pos;
    int remain;
    unsigned char buffer[BUFF_SIZE];
    HIDInterface *hid;
} WMR;


void wmr_send_packet_init(WMR *wmr)
{
    int ret;

    ret = hid_set_output_report(wmr->hid, PATH_IN, PATHLEN, (char*)INIT_PACKET1, sizeof(INIT_PACKET1));
    if (ret != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_set_output_report failed with return code %d\n", ret);
        return;
    }
}

void wmr_send_packet_ready(WMR *wmr)
{
    int ret;
    
    ret = hid_set_output_report(wmr->hid, PATH_IN, PATHLEN, (char*)INIT_PACKET2, sizeof(INIT_PACKET2));
    if (ret != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_set_output_report failed with return code %d\n", ret);
        return;
    }
}

int wmr_init(WMR *wmr)
{
    hid_return ret;
    HIDInterfaceMatcher matcher = { WMR100_VENDOR_ID, WMR100_PRODUCT_ID, NULL, NULL, 0 };
    int retries;

    ret = hid_init();
    if (ret != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_init failed with return code %d\n", ret);
        return 1;
    }

    wmr->hid = hid_new_HIDInterface();
    if (wmr->hid == 0) {
        fprintf(stderr, "hid_new_HIDInterface() failed, out of memory?\n");
        return 1;
    }

    retries = 5;
    while(retries > 0) {
        ret = hid_force_open(wmr->hid, 0, &matcher, 10);
        if (ret == HID_RET_SUCCESS) break;

        fprintf(stderr, "Open failed, sleeping 5 seconds before retrying..\n");
        sleep(5);

        --retries;
    }
    if (ret != HID_RET_SUCCESS) {
      fprintf(stderr, "hid_force_open failed with return code %d\n", ret);
      return 1;
    }
    
    ret = hid_write_identification(stdout, wmr->hid);
    if (ret != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_write_identification failed with return code %d\n", ret);
        return 1;
    }

    wmr_send_packet_init(wmr);
    wmr_send_packet_ready(wmr);
    return 0;
}


int wmr_close(WMR *wmr)
{
    hid_return ret;

    if(wmr == NULL) {
        return 0;
    }

    ret = hid_close(wmr->hid);
    if (ret != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_close failed with return code %d\n", ret);
        return 1;
    }

    hid_delete_HIDInterface(&wmr->hid);
    wmr->hid = NULL;

    ret = hid_cleanup();
    if (ret != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_cleanup failed with return code %d\n", ret);
        return 1;
    }

    return 0;
}

void wmr_read_packet(WMR *wmr)
{
    int ret, len;

    ret = hid_interrupt_read(wmr->hid,
                             USB_ENDPOINT_IN + 1,
                             (char*)wmr->buffer,
                             RECV_PACKET_LEN,
                             0);
    if (ret != HID_RET_SUCCESS) {
        fprintf(stderr, "hid_interrupt_read failed with return code %d\n", ret);
        exit(-1);
        return;
    }
    
    len = wmr->buffer[0];
    if (len > 7) len = 7; /* limit */
    wmr->pos = 1;
    wmr->remain = len;
}

unsigned char wmr_read_byte(WMR *wmr)
{
    while(wmr->remain == 0) { wmr_read_packet(wmr); }
    wmr->remain--;
    return wmr->buffer[wmr->pos++];
}

int verify_checksum(unsigned char * buf, int len) 
{
    int i, ret = 0, chk;
    for (i = 0; i < len -2; ++i) {
        ret += buf[i];
    }
    chk = buf[len-2] + (buf[len-1] << 8);

    if (ret != chk) {
        fprintf(stderr, "Bad checksum: %d / calc: %d\n", ret, chk);
        return -1;
    }
    return 0;
}

/*
void wmr_handle_rain(WMR *wmr, unsigned char *data, int len) 
{
    int sensor, power, rate;
    float hour, day, total;
    int smi, sho, sda, smo, syr;
    char *msg;
    
    sensor = data[2] & 0x0f;
    power = data[2] >> 4;
    rate = data[3];
    
    hour = ((data[5] << 8) + data[4]) * 25.4 / 100.0;  // mm
    day = ((data[7] << 8) + data[6]) * 25.4 / 100.0;   // mm
    total = ((data[9] << 8) + data[8]) * 25.4 / 100.0; // mm

    smi = data[10];
    sho = data[11];
    sda = data[12];
    smo = data[13];
    syr = data[14] + 2000;

    // print
}
*/

void print_temp(unsigned char *data)
{
    float temp, dewpoint;

    temp = (data[3] + ((data[4] & 0x0f) << 8)) / 10.0;
    if(data[4] & 0x80) { temp = -temp; }

    dewpoint = (data[6] + ((data[7] & 0x0f) << 8)) / 10.0;
    if(data[7] & 0x80) { dewpoint = -dewpoint; }

    printf("* sn=%d t=%.1f h=%d d=%.1f pwr=%d\n", 
        data[2] & 0x0f, temp, data[5], dewpoint, data[0] >> 6
    );
}


unsigned char _buffer[20];

void wmr_read_data(WMR *wmr) 
{
    int i, data_len;
    unsigned char b, type;

    unsigned char* data;

    /* search for 0xff marker */
    while((b = wmr_read_byte(wmr)) != 0xff) { }

    /* search for not 0xff */
    while((b = wmr_read_byte(wmr)) == 0xff) { }

    type = wmr_read_byte(wmr);
    
    data_len = 0;   // should be less than read_data_buffer.length
    switch(type) {
        case 0x41: data_len = 17; break;
        case 0x42: data_len = 12; break;
        case 0x44: data_len = 7;  break;
        case 0x46: data_len = 8;  break;
        case 0x47: data_len = 5;  break;
        case 0x48: data_len = 11; break;
        case 0x60: data_len = 12; break;
        default: fprintf(stderr, "Unknown packet type: %02x\n", type);
    }

    if (data_len > 0) {
        _buffer[0] = b; // power
        _buffer[1] = type;
        for (i = 2; i < data_len; ++i) { _buffer[i] = wmr_read_byte(wmr); }

        if (verify_checksum(_buffer, data_len) == 0) 
        {
            data = _buffer;
            switch(type) 
            {
                case 0x41: 
                    // rain: not implemented
                    break;

                case 0x42: 
                    print_temp(data);     
                    break;

                // case 0x44: break; ?water

                case 0x46: 
                    // pressure
                    printf("* p=%d\n", data[2] + ((data[3] & 0x0f) << 8));
                    break;

                case 0x47: 
                    // UVN800 sensor
                    printf("* uv=%d\n", data[3]);
                    break;

                case 0x48:
                    // wind
                    printf("* w=%.1f b=%d r=%d pwr=%d\n", 
                        ((data[5] & 0x0f)*256 + data[4]) / 10.0, 
                        (int)(data[2] & 0x0f) * 360/16, 
                        (data[2] & 0x0f),
                        (data[0] >> 4)
                    );
                    break;

                case 0x60: 
                    // clock
                    printf("* rf=%d pwr=%d\n", ((data[0] >> 4) & 0x3), (data[0] >> 6));            
                    break;
            }    
        }
    }

    wmr_send_packet_ready(wmr);
}


WMR* wmr = NULL;

void cleanup(int sig_num) 
{
    wmr_close(wmr);
    exit(0);
}

int main(int argc, char* argv[]) 
{
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    wmr = malloc(sizeof(WMR));
    wmr->remain = 0;

    if( wmr_init(wmr) != 0) {
        perror("Failed to init USB device, exiting.");
        exit(1);
    }

    while(true) {
        fflush(stdout);
        wmr_read_data(wmr);
    }
}

//.
