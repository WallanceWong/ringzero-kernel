#include "vigil.h"

unsigned int sensor_hr = 0, sensor_spo2 = 0, sensor_temp = 0;

static char line[8];
static unsigned int len = 0;

static unsigned int parse3(const char *s)          /* "072" -> 72 */
{
    unsigned int v = 0;
    for (int i = 0; i < 3; i++)
        v = v * 10 + (s[i] - '0');
    return v;
}

static int digits_ok(const char *s)                /* 1 if all 3 are '0'..'9' */
{
    for (int i = 0; i < 3; i++)
        if (s[i] < '0' || s[i] > '9')
            return 0;
    return 1;
}

void sensor_feed(char c)
{
	if (c == '\b') {
        if (len > 0)
            len--;
        return;
    }
    if (c != '\n') {
        if (len < sizeof(line))
            line[len++] = c;
        return;
    }
    /* Enter pressed: a valid line is exactly 4 characters */
    if (len == 4 && digits_ok(&line[1])) {
        unsigned int v = parse3(&line[1]);
        vigil_on_reading(line[0], (int32_t)v);
        if (line[0] == 'h')
            sensor_hr = v;
        else if (line[0] == 'o')
            sensor_spo2 = v;
        else if (line[0] == 't')
            sensor_temp = v;
    }
    len = 0;
}