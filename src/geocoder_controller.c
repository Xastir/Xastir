/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

/*
 * geocoder_controller.c — Motif-free kernel extracted from geocoder_gui.c.
 *
 * Implements the country table, country lookup helpers, config normalization,
 * and the geocoder_controller_t lifecycle.  No Motif, no X11.
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geocoder_controller.h"

/* ------------------------------------------------------------------
 * Country option table (ISO 3166-1 alpha-2)
 * Moved from the private struct in geocoder_gui.c.
 * Sentinel: {NULL, NULL}
 * ------------------------------------------------------------------ */
typedef struct
{
    const char *label;
    const char *value;
} geocoder_country_option_t;

static const geocoder_country_option_t country_options[] =
{
    {"None (Worldwide)", ""},

    {"Afghanistan", "af"},
    {"Albania", "al"},
    {"Algeria", "dz"},
    {"American Samoa", "as"},
    {"Andorra", "ad"},
    {"Angola", "ao"},
    {"Argentina", "ar"},
    {"Armenia", "am"},
    {"Australia", "au"},
    {"Austria", "at"},
    {"Azerbaijan", "az"},

    {"Bahamas", "bs"},
    {"Bahrain", "bh"},
    {"Bangladesh", "bd"},
    {"Barbados", "bb"},
    {"Belarus", "by"},
    {"Belgium", "be"},
    {"Belize", "bz"},
    {"Bolivia", "bo"},
    {"Bosnia and Herzegovina", "ba"},
    {"Botswana", "bw"},
    {"Brazil", "br"},
    {"Brunei", "bn"},
    {"Bulgaria", "bg"},

    {"Cambodia", "kh"},
    {"Cameroon", "cm"},
    {"Canada", "ca"},
    {"Chile", "cl"},
    {"China", "cn"},
    {"Colombia", "co"},
    {"Costa Rica", "cr"},
    {"Croatia", "hr"},
    {"Cuba", "cu"},
    {"Cyprus", "cy"},
    {"Czech Republic", "cz"},

    {"Denmark", "dk"},
    {"Dominican Republic", "do"},

    {"Ecuador", "ec"},
    {"Egypt", "eg"},
    {"El Salvador", "sv"},
    {"Estonia", "ee"},
    {"Ethiopia", "et"},

    {"Finland", "fi"},
    {"France", "fr"},

    {"Georgia", "ge"},
    {"Germany", "de"},
    {"Ghana", "gh"},
    {"Greece", "gr"},
    {"Greenland", "gl"},
    {"Guatemala", "gt"},

    {"Haiti", "ht"},
    {"Honduras", "hn"},
    {"Hong Kong", "hk"},
    {"Hungary", "hu"},

    {"Iceland", "is"},
    {"India", "in"},
    {"Indonesia", "id"},
    {"Iran", "ir"},
    {"Ireland", "ie"},
    {"Israel", "il"},
    {"Italy", "it"},

    {"Jamaica", "jm"},
    {"Japan", "jp"},
    {"Jordan", "jo"},

    {"Kazakhstan", "kz"},
    {"Kenya", "ke"},
    {"Kuwait", "kw"},

    {"Latvia", "lv"},
    {"Lebanon", "lb"},
    {"Liechtenstein", "li"},
    {"Lithuania", "lt"},
    {"Luxembourg", "lu"},

    {"Malaysia", "my"},
    {"Malta", "mt"},
    {"Mexico", "mx"},
    {"Moldova", "md"},
    {"Monaco", "mc"},
    {"Mongolia", "mn"},
    {"Morocco", "ma"},

    {"Namibia", "na"},
    {"Netherlands", "nl"},
    {"New Zealand", "nz"},
    {"Nigeria", "ng"},
    {"North Macedonia", "mk"},
    {"Norway", "no"},

    {"Oman", "om"},

    {"Pakistan", "pk"},
    {"Panama", "pa"},
    {"Paraguay", "py"},
    {"Peru", "pe"},
    {"Philippines", "ph"},
    {"Poland", "pl"},
    {"Portugal", "pt"},

    {"Qatar", "qa"},

    {"Romania", "ro"},
    {"Russia", "ru"},

    {"Saudi Arabia", "sa"},
    {"Serbia", "rs"},
    {"Singapore", "sg"},
    {"Slovakia", "sk"},
    {"Slovenia", "si"},
    {"South Africa", "za"},
    {"South Korea", "kr"},
    {"Spain", "es"},
    {"Sri Lanka", "lk"},
    {"Sweden", "se"},
    {"Switzerland", "ch"},

    {"Taiwan", "tw"},
    {"Thailand", "th"},
    {"Trinidad and Tobago", "tt"},
    {"Tunisia", "tn"},
    {"Turkey", "tr"},

    {"Ukraine", "ua"},
    {"United Arab Emirates", "ae"},
    {"United Kingdom", "gb"},
    {"United States", "us"},
    {"Uruguay", "uy"},

    {"Venezuela", "ve"},
    {"Vietnam", "vn"},

    {"Zambia", "zm"},
    {"Zimbabwe", "zw"},

    {"--- Custom Code ---", "custom"},
    {NULL, NULL}
};

/* ------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------ */

void geocoder_controller_init(geocoder_controller_t *gc)
{
    if (!gc) return;
    gc->result_count = 0;
}

/* ------------------------------------------------------------------
 * Country table access
 * ------------------------------------------------------------------ */

int geocoder_controller_country_count(void)
{
    int i = 0;
    while (country_options[i].label != NULL) i++;
    return i;
}

const char *geocoder_controller_country_label(int idx)
{
    if (idx < 0) return NULL;
    int i = 0;
    while (country_options[i].label != NULL)
    {
        if (i == idx) return country_options[i].label;
        i++;
    }
    return NULL;
}

const char *geocoder_controller_country_value(int idx)
{
    if (idx < 0) return NULL;
    int i = 0;
    while (country_options[i].label != NULL)
    {
        if (i == idx) return country_options[i].value;
        i++;
    }
    return NULL;
}

int geocoder_controller_find_country_index(const char *value)
{
    if (!value || value[0] == '\0') return 0;

    int i = 0;
    while (country_options[i].label != NULL)
    {
        if (strcmp(country_options[i].value, value) == 0)
            return i + 1;  /* 1-based for XmListSelectPos */
        i++;
    }
    return 0;
}

/* ------------------------------------------------------------------
 * Country code resolution
 * ------------------------------------------------------------------ */

int geocoder_controller_label_to_code(const char *label,
                                       const char *custom_text,
                                       char *out_code,
                                       size_t code_sz,
                                       int *is_custom)
{
    if (out_code && code_sz > 0) out_code[0] = '\0';
    if (is_custom) *is_custom = 0;

    if (!out_code || code_sz == 0) return 0;
    if (!label || label[0] == '\0') return 0;

    /* Search for matching label */
    int i = 0;
    while (country_options[i].label != NULL)
    {
        if (strcmp(country_options[i].label, label) == 0)
        {
            const char *val = country_options[i].value;

            if (strcmp(val, "custom") == 0)
            {
                /* Use custom text field value */
                if (custom_text && custom_text[0] != '\0')
                {
                    snprintf(out_code, code_sz, "%s", custom_text);
                    if (is_custom) *is_custom = 1;
                    return 1;
                }
                /* No custom text supplied → no code */
                return 0;
            }
            else if (val[0] != '\0')
            {
                /* Normal country code */
                snprintf(out_code, code_sz, "%s", val);
                return 1;
            }
            else
            {
                /* Empty value → "None (Worldwide)" — no filter code */
                return 0;
            }
        }
        i++;
    }

    /* Label not found in table */
    return 0;
}

/* ------------------------------------------------------------------
 * Configuration helpers
 * ------------------------------------------------------------------ */

int geocoder_controller_normalize_server_url(char *url, size_t url_sz)
{
    if (!url || url_sz == 0) return 0;

    if (url[0] == '\0')
    {
        snprintf(url, url_sz, "%s", GEOCODER_NOMINATIM_DEFAULT_URL);
        return 1;
    }
    return 0;
}

int geocoder_controller_has_results(const geocoder_controller_t *gc)
{
    if (!gc) return 0;
    return (gc->result_count > 0) ? 1 : 0;
}
