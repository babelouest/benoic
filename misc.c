/**
 *
 * Angharad server
 *
 * Environment used to control home devices (switches, sensors, heaters, etc)
 * Using different protocols and controllers:
 * - Arduino UNO
 * - ZWave
 *
 * Entry point file
 *
 * Copyright 2014-2015 Nicolas Mora <mail@babelouest.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU GENERAL PUBLIC LICENSE
 * License as published by the Free Software Foundation;
 * version 3 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU GENERAL PUBLIC LICENSE for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "angharad.h"

/**
 * Counts the number of digits of a integer
 */
int num_digits (int n) {
  if (n == 0) return 1;
  return floor (log10 (abs (n))) + 1;
}

/**
 * Counts the number of digits of a long
 */
int num_digits_l (long n) {
  if (n == 0) return 1;
  return floor (log10 (abs (n))) + 1;
}

/**
 * Safe string replace function
 * Based on Laird Shaw's replace_str function (http://creativeandcritical.net/str-replace-c/)
 */
int str_replace(const char * source, char * target, size_t len, char * old, char * new) {
  char *r;
  const char *p, *q;
  size_t oldlen = strlen(old);
  size_t count, retlen, newlen = strlen(new);
  
  if (source == NULL || target == NULL || old == NULL) {
    return 0;
  }

  if (oldlen != newlen) {
    for (count = 0, p = source; (q = strstr(p, old)) != NULL; p = q + oldlen)
      count++;
    /* this is undefined if p - source > PTRDIFF_MAX */
    retlen = p - source + strlen(p) + count * (newlen - oldlen);
  } else {
    retlen = strlen(source);
  }

  if (retlen + 1 > len) {
    return 0;
  }

  for (r = target, p = source; (q = strstr(p, old)) != NULL; p = q + oldlen) {
    /* this is undefined if q - p > PTRDIFF_MAX */
    ptrdiff_t l = q - p;
    memcpy(r, p, l);
    r += l;
    memcpy(r, new, newlen);
    r += newlen;
  }
  strcpy(r, p);

  return 1;
}

/**
 * Sanitize special characters for json output
 */
int sanitize_json_string(const char * source, char * target, size_t len) {
  char tmp1[len], tmp2[len];
  unsigned int tab_size = 8, i;
  char *old[] = {"\"", "\\", "/", "\b", "\f", "\n", "\r", "\t"};
  char *new[] = {"\\\"", "\\\\", "\\/", "\\b", "\\f", "\\n", "\\r", "\\t"};
  
  snprintf(tmp1, len, "%s", source);
  for (i = 0; i < tab_size; i++) {
    if (str_replace(tmp1, tmp2, len, old[i], new[i])) {
      snprintf(tmp1, len, "%s", tmp2);
    } else {
      return 0;
    }
  }
  snprintf(target, len, "%s", tmp1);
  return 1;
}

/**
 * Sanitize url for json output
 */
int sanitize_json_string_url(const char * source, char * target, size_t len) {
  char tmp1[len], tmp2[len];
  unsigned int tab_size = 7, i;
  char *old[] = {"\"", "\\", "\b", "\f", "\n", "\r", "\t"};
  char *new[] = {"\\\"", "\\\\", "\\b", "\\f", "\\n", "\\r", "\\t"};
  
  snprintf(tmp1, len, "%s", source);
  for (i = 0; i < tab_size; i++) {
    if (str_replace(tmp1, tmp2, len, old[i], new[i])) {
      snprintf(tmp1, len, "%s", tmp2);
    } else {
      return 0;
    }
  }
  snprintf(target, len, "%s", tmp1);
  return 1;
}

/**
 * Convert a fahrenheit value to celsius and round it to the nearest half value
 * The Celsius value is the only temperature unit stored in angharad
 * So if a Fahrenheit values is read, it's converted and returned as celsius
 */
float fahrenheit_to_celsius(float fahrenheit) {
  return (roundf(2.0*((fahrenheit-32.0)/1.8))/2.0);
}

/**
 * Convert a celsuis value to fahrenheit and round it to the nearest half value
 * The Celsius value is the only temperature unit stored in angharad
 * So if a Fahrenheit values is to be set, it's converted from celsius
 */
float celsius_to_fahrenheit(float celsius) {
  return (roundf(2.0*((celsius*1.8)+32.0))/2.0);
}
