/* guide.c - astronaut Health guide text + a small word-wrap drawing helper.
 * General information only. No clinical ranges are given on purpose:
 * everything is compared with the person's OWN baseline. */
#include "astro.h"

void gfx_draw_string(unsigned int x, unsigned int y, const char *s, unsigned int fg, unsigned int bg);

static const char *const part_names[GUIDE_PARTS] = {
    "WHAT IT IS",
    "WHY WE CHECK IT",
    "HOW SPACEFLIGHT HAZARDS AFFECT IT",
    "WHEN TO ALERT THE CREW OR FLIGHT SURGEON",
    "SAFE GENERAL ACTION",
};

static const char *const titles[GUIDE_PAGES] = {
    "Heart rate", "SpO2 (blood oxygen)", "Blood pressure", "Body temperature",
    "Sleep", "Blood sugar", "Mood", "Cognitive check",
};

/* where the numbers on that page come from (honest source label) */
static const char *const sources[GUIDE_PAGES] = {
    "SIMULATED", "SIMULATED", "SIMULATED", "SIMULATED", "SIMULATED",
    "ENTERED BY THE ASTRONAUT", "ENTERED BY THE ASTRONAUT",
    "MEASURED from you (keyboard tests)",
};

static const char *const text[GUIDE_PAGES][GUIDE_PARTS] = {
    { /* heart rate */
      "How many times your heart beats each minute.",
      "It shows how hard your body is working. A change from your own normal is worth noticing.",
      "Altered gravity moves body fluids and changes how the heart works. Isolation and confinement, and stress, can also change it.",
      "Tell the crew or the flight surgeon if it stays far from your own baseline, or if you feel unwell.",
      "Rest, breathe slowly and measure again later. Follow mission medical procedures." },
    { /* spo2 */
      "The share of oxygen carried by your blood.",
      "It tells us whether your body is getting enough oxygen.",
      "A hostile closed environment (cabin air quality, carbon dioxide build-up) can change it.",
      "Tell the crew at once if it falls below your baseline and you feel short of breath or dizzy.",
      "Check the cabin air alarms, rest, follow mission medical procedures and call the flight surgeon." },
    { /* blood pressure */
      "The force of your blood pushing on the walls of your blood vessels.",
      "A lasting change from your own normal can mean extra strain on the heart and vessels.",
      "Altered gravity moves fluid towards the head. Isolation and confinement can add stress.",
      "Tell the flight surgeon if readings keep rising above your baseline, or come with headache or dizziness.",
      "Sit calmly for a few minutes, measure again and follow mission medical procedures." },
    { /* temperature */
      "How warm your body is.",
      "A change can be an early sign that your body is reacting to something.",
      "A hostile closed environment (shared air and surfaces, cabin heat) can matter. Stress from isolation can too.",
      "Tell the crew and the flight surgeon if it stays away from your baseline, or you feel unwell.",
      "Rest, drink water, measure again later and follow mission medical procedures." },
    { /* sleep */
      "How many hours you sleep.",
      "Sleep lets your body repair and keeps your mind sharp.",
      "Isolation and confinement, noise, light and a changed day-night cycle can cut sleep.",
      "Tell the flight surgeon if you keep sleeping much less than your own normal.",
      "Keep a steady sleep time, dim the lights and follow the mission sleep plan." },
    { /* blood sugar */
      "The amount of sugar in your blood. It gives your body energy.",
      "It shows how your body uses food. Here it is compared only with your own earlier entries.",
      "Altered gravity and less exercise may change how the body uses sugar. Stress can play a part.",
      "Tell the flight surgeon if your entries move far from your baseline, or you feel shaky, very thirsty or confused.",
      "Type in what you measured with the mission-approved device. Follow mission medical procedures." },
    { /* mood */
      "How you feel right now, from 1 (low) to 5 (good).",
      "Mood shows stress and tiredness that numbers can miss.",
      "Isolation and confinement, and distance from Earth and from loved ones, can lower mood. Delayed contact adds to it.",
      "Tell the crew or the flight surgeon if your mood stays low. You do not need to wait.",
      "Talk with a crewmate, keep a routine, plan contact with family and ask for support." },
    { /* cognitive */
      "Short tests of reaction speed, memory and mental math.",
      "They show how alert and focused you are, compared with your own earlier results.",
      "Space radiation, poor sleep, high carbon dioxide in closed cabins and stress may slow thinking. Far from Earth, self-checks matter more.",
      "Tell the flight surgeon if your results stay below your own baseline.",
      "Rest, sleep and repeat the test later. If you feel unfocused, ask the crew before doing risky tasks." },
};

int guide_page_count(void) { return GUIDE_PAGES; }
const char *guide_title(int p)  { return titles[p]; }
const char *guide_source(int p) { return sources[p]; }
const char *guide_part_name(int k) { return part_names[k]; }
const char *guide_text(int p, int k) { return text[p][k]; }

/* Draw s word-wrapped into `cols` columns, one text row every `lh` pixels.
 * Returns the y of the next free row. Never writes outside its columns. */
int draw_wrapped(unsigned int x, unsigned int y, const char *s, unsigned int cols,
                 unsigned int fg, unsigned int bg, unsigned int lh)
{
    char line[128];
    if (cols > 120) cols = 120;
    while (*s) {
        unsigned int n = 0, last_space = 0, i = 0;
        while (s[i] && n < cols) {                 /* take up to cols chars */
            if (s[i] == ' ') last_space = n;
            line[n++] = s[i++];
        }
        if (s[i] && s[i] != ' ' && last_space > 0) {   /* cut at the last space */
            n = last_space;
            i = last_space;
        }
        line[n] = 0;
        gfx_draw_string(x, y, line, fg, bg);
        y += lh;
        s += i;
        while (*s == ' ') s++;
    }
    return (int)y;
}
