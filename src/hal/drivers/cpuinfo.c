/*
Copyright (c) 2012 Ben Croston

Revised by Ernesto Lo Valvo  (ernesto.lovalvo@unipa.it) (12/01/2021)
 Added new version of Raspberry Pi4 and Raspberry Pi 400
 Revised for version 3B (15/01/2021)
 https://www.raspberrypi.org/documentation/hardware/raspberrypi/revision-codes/README.md

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <string.h>

char *get_cpuinfo_revision(char *revision)
{
    FILE *fp;
    char buffer[1024];

    if ((fp = fopen("/sys/firmware/devicetree/base/model", "r")) == NULL)
        return 0;

    fgets(buffer, sizeof(buffer) , fp);

    if (strncmp(buffer, "Raspberry",9) != 0)
        return NULL;
    fclose(fp);

    if ((fp = fopen("/proc/cpuinfo", "r")) == NULL)
        return 0;

    while(!feof(fp)) {
        if (fgets(buffer, sizeof(buffer) , fp)){
            sscanf(buffer, "Revision  : %s", revision);
        }
    }
    fclose(fp);

    return revision;
}


int get_rpi_revision(void)
{
    char revision[1024] = {'\0'};

    if (get_cpuinfo_revision(revision) == NULL)
        return -1;

    if ((strcmp(revision, "0002") == 0) ||
        (strcmp(revision, "1000002") == 0 ) ||
        (strcmp(revision, "0003") == 0) ||
        (strcmp(revision, "1000003") == 0 ))
        return 1;
    else if ((strcmp(revision, "0004") == 0) ||
             (strcmp(revision, "1000004") == 0 ) ||
             (strcmp(revision, "0005") == 0) ||
             (strcmp(revision, "1000005") == 0 ) ||
             (strcmp(revision, "0006") == 0) ||
             (strcmp(revision, "1000006") == 0 ))
        return 2;
    else if ((strcmp(revision, "a01040") == 0) ||   /* Raspberry Pi 2B */
             (strcmp(revision, "a01041") == 0) ||
             (strcmp(revision, "a02042") == 0) ||
             (strcmp(revision, "a21041") == 0) ||
             (strcmp(revision, "a22042") == 0))
        return 3;
    else if ((strcmp(revision, "a02082") == 0) ||   /* Raspberry Pi 3B */
             (strcmp(revision, "a22082") == 0) ||
             (strcmp(revision, "a32082") == 0) ||
             (strcmp(revision, "a52082") == 0) ||
             (strcmp(revision, "a22083") == 0) ||
             (strcmp(revision, "a020d3") == 0))     /* Raspberry Pi 3B+ */
        return 4;
    else if ((strcmp(revision, "a03111") == 0) ||   /* Raspberry Pi 4B */
             (strcmp(revision, "b03111") == 0) ||
             (strcmp(revision, "b03112") == 0) ||
             (strcmp(revision, "b03114") == 0) ||
             (strcmp(revision, "c03111") == 0) ||
             (strcmp(revision, "c03112") == 0) ||
             (strcmp(revision, "c03114") == 0) ||
             (strcmp(revision, "d03114") == 0))
        return 5;
    else if  (strcmp(revision, "c03130") == 0)      /* Raspberry Pi 400 */
        return 6;
    else                                            /* assume rev 7 */
        return 7;
}

