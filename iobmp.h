/* JTAG low-level I/O to Black mahig Debug probes
 
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
*/

#ifndef IOBMP_H
#define IOBMP_H

#include <usb.h>
#include "iobase.h"

#define RAW_JTAG_INIT   1
#define RAW_JTAG_DELAY  2
#define RAW_JTAG_TMS    3
#define RAW_JTAG_TDI    4
#define RAW_JTAG_TDO    5
#define RAW_JTAG_TDITDO 6

#define RAW_JTAG_FINAL 0x80

class IOBMP : public IOBase
{
private:
    char  buffer[1024], *rp;
    int read_len;
    const char *error_str;
    int bmp_open(const char* serial);
    int bmp;
    char get_char(void);
    int bmp_get_packet(int bmp, char* payload, int len);
    int bmp_put_packet(int bmp, const char* cmd, const char* packet, int len);
    int read_buffer(void);
protected:
    FILE *fp_dbg;
public:
    IOBMP(void){};
    int Init(struct cable_t *cable, char const *serial, unsigned int freq);
    ~IOBMP(void);
    void txrx_block(const unsigned char *tdi, unsigned char *tdo, int length, bool last);
    void tx_tms(unsigned char *pat, int length, int force);
    void Usleep(unsigned int usec);
};

#endif // IOBMP_H
