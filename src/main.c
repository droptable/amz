#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "amz.h"

static void usage(void);

int main(int argc, char **argv) 
{
  struct amzres *ares;
  
  if (argc <= 1) {
    usage();
  } else {
    ares = amz_search(argv[1]);
    
    if (ares) {
      printf("url => %s\n", ares->url);
      
      if (ares->cover_src)
        printf("cover_src => %s\n", ares->cover_src);
      
      if (ares->cover_rel)
        printf("cover_rel => %s\n", ares->cover_rel);
      
      if (ares->title) printf("title => %s\n", ares->title);
      if (ares->desc) printf("desc => %s\n", ares->desc);
      
      for (size_t i = 0; i < ares->info.size; ++i) {
        printf("info -> %s => %s\n", 
          ares->info.items[i]->name, 
          ares->info.items[i]->value
        );
      }
      
      amz_clear(ares); 
    } else
      puts("nothing found (error?)");
  }
  
  return 0;
}

static void usage(void)
{
  puts("usage: amz \"search\"");
}
