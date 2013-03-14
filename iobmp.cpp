/* JTAG GNU/Linux Blackmagic debug probe low-level I/O

Copyright (C) 2013 Uwe Bonnes bon@elektron.ikp.physik.tu-darmstadt.de

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*
*
* JTAG transactions are done as monitor commands in the GDB protocoll
* Commands have the form "montitor raw_jtag <PAYLOAD>
* with Payload of the Form
* uint32_t 0xaa550000| (~CMD & 0xff) <<8 | CMD
* uint16_t ticks
* uint8_t[(ticks +7) >> 3] if TDI data has to be transmitted
*
* TODO: Implement multiple PAYLOADS in one monitor raw_jtag packet
*       for less USB turnarounds needed!
*/

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>

#include "iobmp.h"
#define bmp_error_return(code, str) do {                \
         if(IOBMP::verbose) fprintf(stderr,str);        \
         return code;                                   \
     } while(0);

int  unhex(char* rbuf, char* hbuf, int len)
{
    int i;
    for(i = 0; i < len/2; i++)
    {
        char c;
        c = rbuf[2*i + 0];
        if (c > '9')
            c = tolower(c) -'a' +10;
        else
            c -= '0';
        hbuf[i] = c<<4;
        c = rbuf[2*i + 1];
        if (c > '9')
            c = tolower(c) -'a' +10;
        else
            c -= '0';
        hbuf[i] |= c;
    }
    return i;
}

int IOBMP::read_buffer(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    read_len = read(bmp, buffer, 1024);
    buffer[read_len] = 0;
    if (read_len)
    {
        rp = buffer;
    }
    return read_len;
}
char IOBMP::get_char(void)
{
    char c;

    if (read_len == 0 && read_buffer() == 0)
        return -1;
    c = *rp++;
    read_len--;
    return c;
}

/* return length of decode message
 check also for '+'
*/
int IOBMP::bmp_get_packet(int bmp, char* payload, int len)
{
    int c, cc;
    int failures = 0, recv_len;

    while(failures < 10)
    {
        uint8_t csum ;
        do
        {
            c = get_char();
            if (c == '+')
            {
                if (len)
                    payload[0] = c;
                return  1;
            }
            else if (c == -1)
                return c;
        }
        while (c != '$');

        csum =0;
        recv_len = 0;

        do
        {
            c = get_char();

            if (c == '}')
            {
                c = get_char();
                csum += c + '}';
                if (recv_len < len)
                    payload[recv_len] = c ^ 0x20;
                recv_len++;
            }
            else if (c != '#')
            {
                if (recv_len < len)
                    payload[recv_len] = c;
                recv_len++;
                csum += c;
            }
        } while(c != '#');
        c = get_char();
        if (c > '9')
            c = tolower(c) -'a' +10;
        else
            c -= '0';
        cc = c <<4;
        c = get_char();
        if (c > '9')
            c = tolower(c) -'a' +10;
        else
            c -= '0';
         cc |= c;
         if ((cc&0xff) == csum)
            break;
        c = '-';
        write(bmp, &c, 1);
        failures++;
    }
    c = '+';
    write(bmp, &c, 1);
    return recv_len;
}

/* packet wil be hexified, no need to escape */
int IOBMP::bmp_put_packet(int bmp, const char* cmd, const char* packet, int len)
{
    char payload[2048], *p, c;
    const char *q;
    uint8_t checksum = 0;
    int i, j = 0;

    p = payload;
    *p++ = '$';
    for(q = cmd; *q; q++)
    {
        *p++ = *q;
        checksum += *q;
    }
    for(q = packet, i=0; i<len; i++, q++)
    {
        c = ((*q >> 4) & 0xf);
        if (c > 9)
            c = 'a' - 10 + c;
        else
            c = '0' + c;
        *p++ = c;
        checksum += c;
        c = (*q & 0xf);
        if (c > 9)
            c = 'a' - 10 + c;
        else
            c = '0' + c;
        *p++ = c;
        checksum += c;
     }
    *p++ = '#';
    c = ((checksum >> 4) & 0xf);
    if (c > 9)
        *p++ = 'a' - 10 + c;
    else
        *p++ = '0' + c;
    c = (checksum  & 0xf);
    if (c > 9)
        *p++ = 'a' - 10 + c;
    else
        *p++ = '0' + c;
    j = 0;
    while(1)
    {
        char c = 0;
        int k = 0;
        struct timeval tv;

        gettimeofday(&tv, NULL);

        i = write(bmp, payload, p-payload);
        payload[i] = 0;
        if (i != (p-payload))
            fprintf(stderr, "put_packet failure\n");
        do
        {
            i = bmp_get_packet(bmp, &c, 1);
            if (i == -1)
                k++;
        }
        while((i != 1) && (k <2));
        if (k >= 2)
            j++;
        if (c == '+')
        {
            break;
        }
        else if (i)
            fprintf(stderr, "Nack %c\n", c);
        if (j > 10)
        {
            fprintf(stderr, "put_packet repeated failure\n");
            return -1;
        }
    }
    return 0;
}

int IOBMP::bmp_open(const char* serial)
{
    /*
      Use proc filesystem to locate a BMP
    */

    struct dirent **namelist;
    int n, res = -1;
    struct termios termios;
    char full_name[256];

    n = scandir("/dev/serial/by-id/", &namelist, 0, alphasort);
    if (n < 0 )
        return res;
    while(n--)
    {
        if (strncmp(namelist[n]->d_name,
            "usb-Black_Sphere_Technologies_Black_Magic_Probe",
            strlen("usb-Black_Sphere_Technologies_Black_Magic_Probe")) != 0)
            continue;
        if (strstr(namelist[n]->d_name, "if00") == NULL)
            continue;
        if (serial && strlen(serial) && (strstr(namelist[n]->d_name, serial) == NULL))
            continue;
        strcpy(full_name, "/dev/serial/by-id/");
        if ((255 - strlen(full_name)) < strlen(namelist[n]->d_name))
            break;
        strcat(full_name, namelist[n]->d_name);
        res = open(full_name, O_RDWR );
        if (res > -1)
            break;
    }
    free(namelist);
    if (res > -1)
    {
        tcgetattr(res, &termios);
        termios.c_lflag &= ~ICANON; /* Set non-canonical mode */
        termios.c_cc[VTIME] = 1; /* Set timeout of 0.1 seconds */
        if (tcsetattr(res , TCSANOW, &termios) == -1)
        {
            close(res);
            res = -1;
        }
    }
    return res;
}

int IOBMP::Init(struct cable_t *cable, char const *serial, unsigned int freq)
{
    char data[1024];
    char rbuf[1024];
    int r;
    char *fname = getenv("BMP_DEBUG");
    int i;

    if (fname)
        fp_dbg = fopen(fname,"wb");
    else
        fp_dbg = NULL;

    bmp = bmp_open(serial);
    if (bmp < 0)
    {
        fprintf(stderr,"No dongle found\n");
        return -1;
    }
    read_buffer();
    if(!read_len)
        read_buffer();
    while(read_len)
    {
        r = bmp_get_packet(bmp,rbuf, 1024);
    };
    strncpy(data, "raw ", 4);
    i = strlen(data);
    data[i++] = 0xaa;
    data[i++] = 0x55;
    data[i++] = ~RAW_JTAG_INIT;
    data[i++] = RAW_JTAG_INIT;
    data[i++] = 0;
    data[i++] = 1;

    r = bmp_put_packet(bmp, "qRcmd,", data, i);
    r = bmp_get_packet(bmp,rbuf, 1024);
    if ((r <2) || (rbuf[0] != 'O') ||(rbuf[0] == 'K'))
    {
        fprintf(stderr, "Raw JTAG not supported\n");
        return -1;
    }
    return 0;
}

IOBMP::~IOBMP(void)
{
    close(bmp);
    if (fp_dbg)
        fclose(fp_dbg);
}

void IOBMP::Usleep(unsigned int usec)
{
    char data[1024] = "raw ";
    int i = strlen(data);
    if (usec >0xffff)
        usec = 0xffff;
    data[i++] = 0xaa;
    data[i++] = 0x55;
    data[i++] = ~RAW_JTAG_DELAY;
    data[i++] = RAW_JTAG_DELAY;
    data[i++] = usec >> 8;
    data[i++] = usec & 0xff;
    i = bmp_put_packet(bmp, "qRcmd,", data, i);
    i = bmp_get_packet(bmp, data, 1024);
    if ((data[0] != 'O') ||(data[1] == 'K'))
    {
        fprintf(stderr, "Raw JTAG not supported, res %s\n", data);
    }
}

void IOBMP::tx_tms(unsigned char *pat, int length, int force)
{
    char data[1024] = "raw ";
    int j, i = strlen(data);
    if (length > 32)
        length = 32;
    data[i++] = 0xaa;
    data[i++] = 0x55;
    data[i++] = ~RAW_JTAG_TMS;
    data[i++] = RAW_JTAG_TMS;
    data[i++] = (length >> 8 ) & 0xff;
    data[i++] = length & 0xff;
    for(j = 0; j < (length+7)>>3; j++, i++, pat++)
        data[i] = *pat;
    i = bmp_put_packet(bmp, "qRcmd,", data, i);
    i = bmp_get_packet(bmp, data, 1024);
    if ((data[0] != 'O') ||(data[1] != 'K'))
    {
        fprintf(stderr, "tx_tms failed, res %s\n", data);
        return;
    }
}
void IOBMP::txrx_block(const unsigned char *tdi, unsigned char *tdo,
			int length, bool last)
{
    char data[1024] = "raw ";
    int cmd;

    if (tdi || tdo)
        cmd =  RAW_JTAG_TDITDO;
    else if (tdi)
        cmd = RAW_JTAG_TDI;
    else
        cmd = RAW_JTAG_TDO;

    data[4] = 0xaa;
    data[5] = 0x55;
    do
    {
        char hexdata[1024];
        int i, k, current_len;
        if (length > 0xffff)
        {
            current_len = 0xffff;
            k = 0xffff>>3;
        }
        else
        {
            current_len = length;
            if (last)
                cmd |= RAW_JTAG_FINAL;
        }
        k = (current_len +7)>>3;
        data[6] = ~cmd;
        data[7] = cmd;
        data[8] = (current_len >> 8 ) & 0xff;
        data[9] = current_len & 0xff;
        if (tdi)
        {
            memcpy(data + 10, tdi, k);
            tdi += k;
        }
        i = bmp_put_packet(bmp, "qRcmd,", data, 10 +k);
        if (tdo)
        {
            unsigned char * wp = tdo;
            do
            {
                i = bmp_get_packet(bmp, data, 1024);
                if (data[0] != 'O')
                {
                    fprintf(stderr, "tx_tms failed\n");
                    return ;
                }
                i = unhex(data+1, hexdata, i-1);
                memcpy(wp, hexdata, i);
                k -= i;
                wp += i;
            }
            while(k > 0);
        }
        /* get OK packet */
        i = bmp_get_packet(bmp, data, 1024);
        if ((data[0] != 'O') ||(data[1] != 'K'))
        {
            fprintf(stderr, "TDx failed\n");
            return;
        }
        length =- current_len;
    }
    while(length> 0);
}
