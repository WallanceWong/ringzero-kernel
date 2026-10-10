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

void sensor_feed(char c)
{
    if (c != '\n') {
        if (len < sizeof(line))
            line[len++] = c;
        return;
    }
    /* Enter pressed: a valid line is exactly 4 characters */
    if (len == 4) {
        unsigned int v = parse3(&line[1]);
        if (line[0] == 'h')
            sensor_hr = v;
        else if (line[0] == 'o')
            sensor_spo2 = v;
        else if (line[0] == 't')
            sensor_temp = v;
    }
    len = 0;
}