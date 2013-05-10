#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <curl/curl.h>
#include <pcre.h>
#include <iconv.h>

#include "amz.h"
#include "url.h"
#include "mem.h"
#include "strbuf.h"
#include "htmlent.h"

/* --------------------------------- */

static char *
latin9_to_utf8(char *str)
{
  iconv_t ic = iconv_open("UTF-8", "ISO-8859-15");
  char *res, *ptr;
  size_t ret;
  
  if (!ic) {
    fputs("iconv error\n", stderr);
    exit(1);
  }
  
  size_t slen = strlen(str);
  size_t rlen = slen * sizeof(int); /* avoid realloc */
  size_t clen = rlen;
  
  /* worstcase alloc ... */
  res = amz_calloc(1, rlen + 1);
  ptr = res;
  ret = iconv(ic, &str, &slen, &res, &rlen);
  
  if (ret == -1) {
    fprintf(stderr, "iconv error: %d\n", errno);
    exit(1);
  }
  
  iconv_close(ic);
  
  ptr = amz_realloc(ptr, 1 + clen - rlen);
  return ptr;
}

/* --------------------------------- */

static inline char *
strmerge(char *a, char *b)
{
  size_t len = strlen(b);
  
  a = amz_realloc(a, strlen(a) + len + 1);
  strncat(a, b, len);
  
  return a;
}

static char *
strstriptags(char *d)
{
  bool in_tag = false;
  struct strbuf *buf = strbuf_init();
  char *res;
  
  /* skip leading whitespace because i'm too lazy to implement ltrim() */
  for (; *d; ++d) if (!isspace(*d)) break;
  
  while (*d) {
    char c = *d++;
    
    if (in_tag) {
      if (c == '>') in_tag = false;
      continue;
    } else if (c == '<') {
      in_tag = true;
      continue;
    }
    
    strbuf_addc(buf, c);
  }
  
  res = strbuf_cstr(buf);
  strbuf_free(buf);
  
  return res;
}

static char *
strtrim(char *str)
{
  size_t len = strlen(str);
  size_t so = 0;
  size_t eo = len - 1;
  char *res;
  
  /* todo: avoid strbuf here you lazy bastard */
  struct strbuf *buf;
  
  for (; str[so]; ++so) if (!isspace(str[so])) break;
  for (; str[eo]; --eo) if (!isspace(str[eo])) break;
  
  if (so == 0 && eo == len)
    return NULL;
  
  buf = strbuf_init();
  
  for (size_t i = so; i <= eo; ++i)
    strbuf_addc(buf, str[i]);
  
  res = strbuf_cstr(buf);
  strbuf_free(buf);
  
  return res;
}

/* --------------------------------- */

struct pregres {
  size_t size;
  char **groups;
  
  /* only used in "global" matches */ 
  struct pregres *next;
};

static struct pregres *
preg_match_impl(char *regex, const char *subject, int flags, bool global)
{
  pcre *re;
  pcre_extra *ex = NULL;
  
  const char *pcerror;
  int pcerroff;
  int ovector[30];
  int offset = 0;
  int options = 0;
  
  size_t slen = strlen(subject);
  
  struct pregres *list = NULL, *item, *prev;
  
  re = pcre_compile(regex, flags, &pcerror, &pcerroff, NULL);
  
  if (!re) {
    fputs("error pcre_compile(),", stderr);
    exit(1);
  }
  
  if (global) {
    ex = pcre_study(re, 0, &pcerror);
    
    if (!ex) {
      fputs("error pcre_study()", stderr);
      exit(1);
    }
  }
  
  do {
    int rc = pcre_exec(re, ex, subject, slen, 
      offset, options, ovector, 30);
    
    options |= PCRE_NO_UTF8_CHECK;
    
    if (global && (rc == PCRE_ERROR_NOMATCH)) {
      if ((options & PCRE_NOTEMPTY) && offset < slen) {
        ++offset;
        continue;
      }
      
      break;
    }
    
    if (rc < 0) {
      fprintf(stderr, "error pcre_exec(): %d\n", rc);
      exit(1);
    }
    
    item = amz_alloc(sizeof(struct pregres));
    item->size = 0;
    item->next = NULL;
    item->groups = amz_alloc(rc * sizeof(char*));
    
    for (int i = 0; i < rc; ++i) {
      int ss = ovector[2 * i];
      int sl = ovector[2 * i + 1] - ss;
      
      char *m = amz_calloc(1, sl + 1);
      strncpy(m, subject + ss, sl);
      
      item->groups[item->size++] = m;
    }
    
    if (list == NULL)
      list = item;
    else
      prev->next = item;
    
    prev = item;
    
    if (ovector[0] == ovector[1])
      options = PCRE_NOTEMPTY | PCRE_ANCHORED;
    
    offset = ovector[1];
  } while (global);
  
  if (global) 
    pcre_free_study(ex);
  
  pcre_free(re);
  return list;
}

static inline struct pregres *
preg_match(char *regex, const char *subject, int flags)
{
  return preg_match_impl(regex, subject, flags, false);
}

static inline struct pregres *
preg_match_all(char *regex, const char *subject, int flags)
{
  return preg_match_impl(regex, subject, flags, true);
}

static void
pregres_free(struct pregres *rm)
{
  struct pregres *item = rm, *next;
  
  while (item) {
    while (item->size) 
      amz_free(item->groups[--item->size]);
      
    amz_free(item->groups);
    
    next = item->next;
    amz_free(item);
    
    item = next;
  }
}

/* --------------------------------- */

struct amzreq {
  CURL *ceasy;
  struct curl_slist *headers;
};

static size_t
amzreq_recv(void *data, size_t size, size_t nmemb, void *up)
{
  size_t rs = size * nmemb;
  struct strbuf *buf = up;
  
  strbuf_addn(buf, (char *) data, rs);
  return rs;
}

#define AMZ_UA "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; " \
               "rv:20.0) Gecko/20100101 Firefox/20.0"

#define AMZ_HAC_LANG "Accept-Language: de-de,de;q=0.8,en-us;q=0.5,en;q=0.3"
#define AMZ_HAC "Accept: text/html,application/xhtml+xml,application/xml;" \
                "q=0.9,*/*;q=0.8" 

/* github highlighter bug */

#define AMZ_COOKIES "./amz_cookies.dat"

static struct amzreq *
amzreq_init(const char *url, const char *ref)
{
  CURL *ch;
  struct curl_slist *hs;
  struct amzreq *req;
  
  if (!(ch = curl_easy_init())) {
    fputs("error @ setup_curl (curl_easy_init)", stderr);
    exit(1);
  }
  
  curl_easy_setopt(ch, CURLOPT_URL, url);
  if (ref) curl_easy_setopt(ch, CURLOPT_REFERER, ref);
  
  curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, true);
  curl_easy_setopt(ch, CURLOPT_ENCODING, "");
  curl_easy_setopt(ch, CURLOPT_USERAGENT, AMZ_UA);
  curl_easy_setopt(ch, CURLOPT_COOKIEFILE, AMZ_COOKIES);
  curl_easy_setopt(ch, CURLOPT_COOKIEJAR, AMZ_COOKIES);
  curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, amzreq_recv);
  //curl_easy_setopt(ch, CURLOPT_VERBOSE, true);
  
  hs = NULL;
  hs = curl_slist_append(hs, AMZ_HAC_LANG);
  hs = curl_slist_append(hs, AMZ_HAC);
  
  curl_easy_setopt(ch, CURLOPT_HTTPHEADER, hs);
  
  req = amz_alloc(sizeof(struct amzreq));
  req->ceasy = ch;
  req->headers = hs;
  
  return req;
}

static char *
amzreq_exec(struct amzreq *req)
{
  CURLcode res;
  
  char *data;
  struct strbuf *buf;
  buf = strbuf_init();
  
  curl_easy_setopt(req->ceasy, CURLOPT_WRITEDATA, buf);
  res = curl_easy_perform(req->ceasy);
  
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n", 
      curl_easy_strerror(res));
    exit(1);
  }
  
  data = strbuf_cstr(buf);
  strbuf_free(buf);
  
  return data;
}

static void
amzreq_free(struct amzreq *req)
{
  if (!req) return;
  if (req->ceasy) curl_easy_cleanup(req->ceasy);
  if (req->headers) curl_slist_free_all(req->headers);
  amz_free(req);
}

/* --------------------------------- */

static void
amzinfo_init(struct amzres *res)
{
  res->info.size = 0;
  res->info.items = NULL;
}

static void 
amzinfo_add(struct amzres *res, char *name, char *value)
{
  size_t size = res->info.size;
  
  struct amzinfo **items;
  items = res->info.items;
  
  items = amz_realloc(items, sizeof(struct amzinfo *) * (size + 1));
  items[size] = amz_alloc(sizeof(struct amzinfo));
  items[size]->name = name;
  items[size]->value = value;
  ++size;
  
  res->info.items = items;
  res->info.size = size;
}

/* --------------------------------- */

static struct amzres *
amzres_init()
{
  struct amzres *res;
  
  res = amz_alloc(sizeof(struct amzres));
  res->url = NULL;
  res->cover_src = NULL;
  res->cover_rel = NULL;
  res->title = NULL;
  res->desc = NULL;
  res->info.size = 0;
  res->info.items = NULL;
  
  return res;
}

static void
amzres_free(struct amzres *res)
{
  if (res->cover_src) amz_free(res->cover_src);
  if (res->cover_rel) amz_free(res->cover_rel);
  if (res->title) amz_free(res->title);
  if (res->desc) amz_free(res->desc);
  
  while (res->info.size) {
    amz_free(res->info.items[--res->info.size]->name);
    amz_free(res->info.items[res->info.size]->value);
    amz_free(res->info.items[res->info.size]);
  }
  
  if (res->url) amz_free(res->url);
  amz_free(res);
}

static void 
amzres_fetch_cover(struct amzres *res, const char *body)
{
  static char re[] = "(<img\\s+.*?id=\"main-image\"[^>]*>)";
  static char re_src[] = "src=\"([^\"]+)\"";
  static char re_rel[] = "rel=\"([^\"]+)\"";
  
  struct pregres *m;
  char *src = NULL, *rel = NULL;
  char *img;
  
  if (!(m = preg_match(re, body, 0)))
    return;
  
  img = strdup(m->groups[1]);
  pregres_free(m);
  
  if ((m = preg_match(re_src, img, 0))) {
    src = strdup(m->groups[1]);
    pregres_free(m);
  }
  
  if ((m = preg_match(re_rel, img, 0))) {
    rel = strdup(m->groups[1]);
    pregres_free(m);
  }
  
  res->cover_src = src;
  res->cover_rel = rel;
  
  free(img);
}

static void 
amzres_fetch_title(struct amzres *res, const char *body)
{
  static char re[] = "<span\\s+id=\"btAsinTitle\">([^<]+)";
  
  struct pregres *m;
  char *title_tmp;
  char *title_utf8;
  
  if (!(m = preg_match(re, body, 0)))
    return;
  
  title_tmp = strdup(m->groups[1]);
  pregres_free(m);
  
  title_utf8 = htmlent_decode(title_tmp);
  
  res->title = title_utf8;
  
  free(title_tmp);
}

static void 
amzres_fetch_desc(struct amzres *res, const char *body)
{
  static char re[] = "<div\\s+class=\"productDescriptionWrapper\"[^>]*>(.*?)<div";
  static int flags = PCRE_DOTALL | PCRE_MULTILINE;
  
  struct pregres *m;
  char *desc_tmp1;
  char *desc_tmp2;
  char *desc_utf8;
  
  if (!(m = preg_match(re, body, flags)))
    return;
  
  desc_tmp1 = strdup(m->groups[1]);
  pregres_free(m);
  
  desc_tmp2 = strstriptags(desc_tmp1);    
  desc_utf8 = htmlent_decode(desc_tmp2);
  
  res->desc = desc_utf8;
  
  amz_free(desc_tmp1);
  amz_free(desc_tmp2);
}

static void 
amzres_fetch_info(struct amzres *res, const char *body)
{
  static char re_ul[] = "<a\\s+.*?id=\"productDetails\"[^>]*>\\s*</a>.*?<ul>(.*?)</ul>";
  static int re_ul_flags = PCRE_DOTALL;
  
  static char re_li[] = "^\\s*<li\\s*>\\s*<b\\s*>([^<]+?)\\s*:?</b>([^<]+?)</li>";
  static int re_li_flags = PCRE_DOTALL | PCRE_MULTILINE;
  
  struct pregres *m;
  struct pregres *i;
  char *ul;
  
  if (!(m = preg_match(re_ul, body, re_ul_flags)))
    return;
  
  ul = strdup(m->groups[1]);
  pregres_free(m);
  
  if (!(m = preg_match_all(re_li, ul, re_li_flags))) {
    free(ul);
    return;
  }
  
  free(ul);
  
  amzinfo_init(res);
  
  /* hey, the next part is ugly, please skip it */
  
  for (i = m; i; i = i->next) {
    char *name_tmp = strdup(i->groups[1]);
    char *name_utf8 = latin9_to_utf8(name_tmp);
    char *name_final;
    amz_free(name_tmp);
    
    char *value_tmp = strdup(i->groups[2]);
    char *value_utf8 = latin9_to_utf8(value_tmp);
    char *value_final;
    free(value_tmp);
    
    if (!(name_final = strtrim(name_utf8)))
      name_final = strdup(name_utf8);
    
    if (!(value_final = strtrim(value_utf8)))
      value_final = strdup(value_utf8);
    
    amz_free(name_utf8);
    amz_free(value_utf8);
    
    amzinfo_add(res, name_final, value_final);
  }
  
  /* using flash-thingy *zzzz* nothing happend, go ahead */
  
  pregres_free(m);
}

/* --------------------------------- */

#define AMZ_SREF "http://www.amazon.de/ref=gno_logo"
#define AMZ_SURL "http://www.amazon.de/s/ref=nb_sb_noss?__" \
                 "mk_de_DE=%C3%85M%C3%85Z%C3%95%C3%91&url=search-alias%3Daps"

AMZAPI struct amzres *
amz_search(const char *term)
{
  static char re[] = 
    "<div\\s+.*?id=\"atfResults\"[^>]*>.*?<a\\s+.*?href=\"([^\"]+)\"";
                     
  char *url, *durl, *utm, *murl, *body;
  struct amzres *res;
  struct amzreq *req;
  struct pregres *m;
  
  url = strdup(AMZ_SURL "&field-keywords=");
  utm = url_encode((char *) term);
  
  url = strmerge(url, utm);
  amz_free(utm);
  
  req = amzreq_init(url, AMZ_SREF);
  body = amzreq_exec(req);
  amzreq_free(req);
  amz_free(url);
  
  if (!(m = preg_match(re, body, PCRE_DOTALL)))
    return NULL;
  
  murl = strdup(m->groups[1]);
  durl = htmlent_decode(murl);
  pregres_free(m);
  amz_free(murl);
  amz_free(body);
  
  res = amz_fetch(durl);
  amz_free(durl);
  
  return res;
}

#define AMZ_FREF "http://www.amazon.de/s/ref=nb_sb_noss?" \
                 "mk_de_DE=%C3%85M%C3%85Z%C3%95%C3%91&url=search-alias%3Daps"

AMZAPI struct amzres *
amz_fetch(const char *url)
{
  char *body;
  struct amzreq *req;
  struct amzres *res;
  
  req = amzreq_init(url, AMZ_FREF);
  body = amzreq_exec(req);
  amzreq_free(req);
  
  res = amzres_init();
  res->url = strdup(url);
  
  amzres_fetch_cover(res, body);
  amzres_fetch_title(res, body);
  amzres_fetch_desc(res, body);
  amzres_fetch_info(res, body);
  
  return res;
}

AMZAPI void 
amz_clear(struct amzres *res)
{
  amzres_free(res);
}

