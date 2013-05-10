#ifndef _HAS_URL_H
#define _HAS_URL_H

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(char *str);

/* Returns a url-decoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_decode(char *str);

#endif
