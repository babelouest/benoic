/**
 *
 * Angharad server
 *
 * Environment used to control home devices (switches, sensors, heaters, etc)
 * Using different protocols and controllers:
 * - Arduino UNO
 * - ZWave
 *
 * Copyright 2014-2015 Nicolas Mora <mail@babelouest.org>
 * Gnu Public License V3 <http://fsf.org/>
 *
 * Entry point file
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
int str_replace(char * source, char * target, size_t len, char * old, char * new) {
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
int sanitize_json_string(char * source, char * target, size_t len) {
  char tmp1[len], tmp2[len];
  unsigned int tab_size = 8, i;
  char *old[] = {"\\", "\b", "\f", "\n", "\r", "\t", "\v", "\""};
  char *new[] = {"\\\\", "\\b", "\\f", "\\n", "\\r", "\\t", "\\v", "\\\""};
  
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
