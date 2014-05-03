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
    refactored by m@penzin.ru
*/

#include <hid.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>

#define WMR100_VENDOR_ID  0x0fde
#define WMR100_PRODUCT_ID 0xca01

#define MAXSENSORS 5
#define RECORD_HISTORY 60

int const RECV_PACKET_LEN   = 8;
int const BUF_SIZE = 255;

unsigned char const PATHLEN = 2;
int const PATH_IN[]  = { 0xff000001, 0xff000001 };
int const PATH_OUT[] = { 0xff000001, 0xff000002 };

unsigned char const INIT_PACKET1[] = { 0x20, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00 };
unsigned char const INIT_PACKET2[] = { 0x01, 0xd0, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00 };

typedef struct _WMR {
    int pos;
    int remain;
    unsigned char* buffer;
    HIDInterface *hid;
    // FILE *data_fh;
    // char *data_filename;
    // void *zmq_ctx;
    // void *zmq_sock;
} WMR;

WMR *wmr = NULL;


void dump_packet(unsigned char *packet, int len)
{
    int i;

    printf("Receive packet len %d: ", len);
    for(i = 0; i < len; ++i) {
        printf("%02x ", (int)packet[i]);
    }
    printf("\n");
}


WMR *wmr_new()
{
    WMR *wmr = malloc(sizeof(WMR));
    wmr->remain = 0;
    wmr->buffer = malloc(BUF_SIZE);
    if (wmr->buffer == NULL) {
      free(wmr);
      return NULL;
    }
    return wmr;
}

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

    /* see include/debug.h for possible values */
    /*hid_set_debug(HID_DEBUG_ALL);*/
    /*hid_set_debug_stream(stderr);*/
    /* passed directly to libusb */
    /*hid_set_usb_debug(0);*/
  
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
    while(wmr->remain == 0) {
        wmr_read_packet(wmr);
    }
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
        printf("Bad checksum: %d / calc: %d\n", ret, chk);
        return -1;
    }
    return 0;
}


void wmr_log_data(WMR *wmr, char *topic, char *msg) 
{
    // char timestamp[200];
    char *buf;
    // struct tm *tmp;
    // struct timeval tv;

    // gettimeofday(&tv, NULL);
    // tmp = gmtime(&tv.tv_sec);

    // strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tmp);
    asprintf(&buf,
        "{\"topic\": \"%s\", "
        // "\"timestamp\": \"%s.%06d\", "
        "%s}",
        topic,
        // timestamp,
        // (int)tv.tv_usec,
        msg);

    printf("%s\n", buf);
    free(buf);
}

/****************************
  Data handlers
 ****************************/

void wmr_handle_rain(WMR *wmr, unsigned char *data, int len) 
{
    // NOTE: function disabled
    return; 
    //

    int sensor, power, rate;
    float hour, day, total;
    int smi, sho, sda, smo, syr;
    char *msg;
    
    sensor = data[2] & 0x0f;
    power = data[2] >> 4;
    rate = data[3];
    
    hour = ((data[5] << 8) + data[4]) * 25.4 / 100.0; /* mm */
    day = ((data[7] << 8) + data[6]) * 25.4 / 100.0; /* mm */
    total = ((data[9] << 8) + data[8]) * 25.4 / 100.0; /* mm */

    smi = data[10];
    sho = data[11];
    sda = data[12];
    smo = data[13];
    syr = data[14] + 2000;

    asprintf(&msg,
             "\"sensor\": %d, "
             "\"power\": %d, "
             "\"rate\": %d, "
             "\"hour_total\": %.2f, "
             "\"day_total\": %.2f, "
             "\"all_total\": %.2f, "
             "\"since\": \"%04d%02d%02d%02d%02d\", "
             "\"source\": \"wmr100.%d\", "
             "\"origin\": \"wmr100\"",
             sensor, power, rate, hour, day, total, syr, smo, sda, sho, smi, sensor);
    wmr_log_data(wmr, "rain", msg);
    free(msg);
}

void wmr_handle_temp(WMR *wmr, unsigned char *data, int len)
{
    int sensor, humidity; // st, smiley, trend,
    float temp, dewpoint;
    // // char *msg;

    sensor = data[2] & 0x0f;
    // st = data[2] >> 4;
    // smiley = st >> 2;
    // trend = st & 0x03;
    // trend -= 1;

    temp = (data[3] + ((data[4] & 0x0f) << 8)) / 10.0;
    if ((data[4] >> 4) == 0x8) { temp = -temp; }
    
    humidity = data[5];

    dewpoint = (data[6] + ((data[7] & 0x0f) << 8)) / 10.0;
    if ((data[7] >> 4) == 0x8) { dewpoint = -dewpoint; }

    printf("=S%d t=%.1f h=%d d=%.1f", sensor, temp, humidity, dewpoint);

    // asprintf(&msg,
    //          "\"sensor\": %d, "
    //          "\"smile\": %d, "
    //          "\"trend\": %d, "
    //          "\"temp\": %.1f, "
    //          "\"humidity\": %d, "
    //          "\"dewpoint\": %.1f, "
    //          "\"source\": \"wmr100.%d\", "
    //          "\"origin\": \"wmr100\"",
    //          sensor, smiley, trend, temp, humidity, dewpoint, sensor);
    // wmr_log_data(wmr, "temp", msg);
    // free(msg);
}

void wmr_handle_water(WMR *wmr, unsigned char *data, int len)
{
    int sensor;
    float temp;
    char *msg;

    sensor = data[2] & 0x0f;

    temp = (data[3] + ((data[4] & 0x0f) << 8)) / 10.0;
    if ((data[4] >> 4) == 0x8)
        temp = -temp;

    asprintf(&msg,
             "\"sensor\": %d, "
             "\"temp\": %.1f, "
             "\"source\": \"wmr100\", "
             "\"origin\": \"wmr100\"",
             sensor, temp);
    wmr_log_data(wmr, "water", msg);
    free(msg);
}

void wmr_handle_pressure(WMR *wmr, unsigned char *data, int len)
{
    int pressure, forecast, alt_pressure, alt_forecast;
    char *msg;

    pressure = data[2] + ((data[3] & 0x0f) << 8);
    forecast = data[3] >> 4;
    alt_pressure = data[4] + ((data[5] & 0x0f) << 8);
    alt_forecast = data[5] >> 4;

    asprintf(&msg,
             "\"pressure\": %d, "
             "\"forecast\": %d, "
             "\"altpressure\": %d, "
             "\"altforecast\": %d, "
             "\"source\": \"wmr100\", "
             "\"origin\": \"wmr100\"",
             pressure, forecast, alt_pressure, alt_forecast);
    wmr_log_data(wmr, "pressure", msg);
    free(msg);
}

void wmr_handle_uv(WMR *wmr, unsigned char *data, int len)
{
    // NOTE: not implemented

    // char *msg;

    // asprintf(&msg, "\"todo\": 1");
    // wmr_log_data(wmr, "uv", msg);
    // free(msg);
}

void wmr_handle_wind(WMR *wmr, unsigned char *data, int len)
{
    char *msg;
    int wind_dir, power, low_speed, high_speed;
    float wind_speed, avg_speed;

    wind_dir = data[2] & 0xf;
    power = data[2] >> 4;
    
    wind_speed = data[4] / 10.0;

    low_speed = data[5] >> 4;
    high_speed = data[6] << 4;
    avg_speed = (high_speed + low_speed) / 10.0;

    asprintf(&msg,
             "\"power\": %d, "
             "\"dir\": %d, "
             "\"speed\": %.1f, "
             "\"avgspeed\": %.1f, "
             "\"source\": \"wmr100\", "
             "\"origin\": \"wmr100\"",
             power, wind_dir, wind_speed, avg_speed);
    wmr_log_data(wmr, "wind", msg);
    free(msg);
}

void wmr_handle_clock(WMR *wmr, unsigned char *data, int len)
{
    if(data[0]) {
        printf("=S0 pwr=%02x", data[0])
    }

    // int power, powered, battery, rf, level, mi, hr, dy, mo, yr;
    // char *msg;

    // power = data[0] >> 4;
    // powered = power >> 3;
    // battery = (power & 0x4) >> 2;
    // rf = (power & 0x2) >> 1;
    // level = power & 0x1;

    // mi = data[4];
    // hr = data[5];
    // dy = data[6];
    // mo = data[7];
    // yr = data[8] + 2000;

    // asprintf(&msg,
    //          "\"at\": \"%04d%02d%02d%02d%02d\", "
    //          "\"powered\": %d, "
    //          "\"battery\": %d, "
    //          "\"rf\": %d, "
    //          "\"level\": %d, "
    //          "\"source\": \"wmr100\", "
    //          "\"origin\": \"wmr100\"",
    //          yr, mo, dy, hr, mi, powered, battery, rf, level);
    // wmr_log_data(wmr, "clock", msg);
    // free(msg);
}

unsigned char _buffer[20];

void wmr_read_data(WMR *wmr) 
{
    int i, data_len;
    unsigned char b, type;

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

        if (verify_checksum(_buffer, data_len) == 0) {
            switch(type) {
                case 0x41: wmr_handle_rain(wmr, _buffer, data_len);     break;
                case 0x42: wmr_handle_temp(wmr, _buffer, data_len);     break;
                case 0x44: wmr_handle_water(wmr, _buffer, data_len);    break;
                case 0x46: wmr_handle_pressure(wmr, _buffer, data_len); break;
                case 0x47: wmr_handle_uv(wmr, _buffer, data_len);       break;
                case 0x48: wmr_handle_wind(wmr, _buffer, data_len);     break;
                case 0x60: wmr_handle_clock(wmr, _buffer, data_len);    break;
            }    
        }
    }

    wmr_send_packet_ready(wmr);
}


void cleanup(int sig_num) 
{
    wmr_close(wmr);
    exit(0);
}

int main(int argc, char* argv[]) 
{
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    wmr = wmr_new();
    if (wmr == NULL) {
        perror("wmr_new failed");
        exit(1);
    }

    if( wmr_init(wmr) != 0) {
        perror("Failed to init USB device, exiting.");
        exit(1);
    }

    printf("Found on USB: %s\n", wmr->hid->id);

    fprintf(stderr, "WMR: HID: %p\n", (void *)wmr->hid);

    while(true) {
        wmr_read_data(wmr);
    }
 
    return 0;
}

//.
