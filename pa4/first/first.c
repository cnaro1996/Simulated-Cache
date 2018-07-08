#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<math.h>

/*Contains a block (array of long addresses) and valid bit*/
struct line{
  /*Valid bit: 0 or 1*/
  int validbit;
  unsigned long tagbits;
  int entryorder;/*fifo*/
  int usedorder; /*lru*/
};

/*Array of lines*/
struct set{
  struct line* lines;
};

/*Array of sets, contains replacement policy*/
struct cache{
  struct set* sets;
  char* policy;
  char* assoc;
  int block_size;
  int cache_size;
  int assoc_n;
};

/*Initialize a cache*/
struct cache* InitializeCache(int cache_size, int assoc_n, char* assoc, char* cache_pol, int block_size,
                              double* s, double* b, double* t){
  struct cache* cache1;
  int set_n, i, j;
  
  cache1 = (struct cache*) malloc(sizeof(struct cache));
  cache1->policy = cache_pol;
  cache1->assoc = assoc;
  cache1->block_size = block_size;
  cache1->cache_size = cache_size;

  /*Initialize cache based on Associativity*/ 
  if(strcmp(assoc, "direct") == 0){ /*Direct Mapping cache*/

    cache1->assoc_n = 1;
    set_n = cache_size/block_size;
    cache1->sets = (struct set*) malloc(sizeof(struct set) * set_n);

    /*Initialize each set's line, and also each line's tag and valid bits to 0, entry order to -1*/
    for(i = 0; i<set_n; i++){
      cache1->sets[i].lines = (struct line*) malloc(sizeof(struct line));
      cache1->sets[i].lines[0].validbit = 0;
      cache1->sets[i].lines[0].tagbits = 0;
      cache1->sets[i].lines[0].entryorder = -1;
      cache1->sets[i].lines[0].usedorder = -1;
    }

    /*Calculate # of set index bits, block-offset bits, and tag bits*/
    *s = log(set_n)/log(2);
    *b = log(block_size)/log(2);
    *t = 48-(*s+*b);

  }else if(strcmp(assoc, "assoc") == 0){ /*Fully Associative cache*/

    cache1->sets = (struct set*) malloc(sizeof(struct set));
    assoc_n = cache_size/block_size;
    cache1->assoc_n = assoc_n;
    cache1->sets->lines = malloc(sizeof(struct line) * assoc_n);

    /*For each line, initiate tag and valid bit to 0, entry order to -1*/
    for(i = 0; i<assoc_n; i++){
      cache1->sets[0].lines[i].tagbits = 0;
      cache1->sets[0].lines[i].validbit = 0;
      cache1->sets[0].lines[i].entryorder = -1;
      cache1->sets[0].lines[i].usedorder = -1;
    }

    /*Calculate set # of index bits, block-offset bits, and tag bits*/
    *s = 0;
    *b = log(block_size)/log(2);
    *t = 48-*b;

  }else{ /*N-way Associative cache, where N = assoc_n*/
    
    set_n = cache_size/(assoc_n*block_size);
    cache1->assoc_n = assoc_n;
    cache1->sets = (struct set*) malloc(sizeof(struct set) * set_n);

    /*For each set, initialize an array of lines*/
    for(i = 0; i<set_n; i++){
      cache1->sets[i].lines = (struct line*) malloc(sizeof(struct line) * assoc_n);
      /*For each line, initialize tag bit, valid bit, and entry order*/
      for(j = 0; j<assoc_n; j++){
        cache1->sets[i].lines[j].validbit = 0;
        cache1->sets[i].lines[j].tagbits = 0;
        cache1->sets[i].lines[j].entryorder = -1;
        cache1->sets[i].lines[j].usedorder = -1;
      }
    }

     /*Calculate set # of index bits, block-offset bits, and tag bits*/
     *s = log(set_n)/log(2);
     *b = log(block_size)/log(2);
     *t = 48-(*s+*b);
  }
  return cache1;
}

/*creates a mask, from bits a to b*/
unsigned long createMask(unsigned long a, unsigned long b){
  unsigned long i;
  unsigned long r = 0;

  for (i=a; i<=b; i++){
      r |= 1 << i;
  }

   return r;
}

/*Use these statements in order to update all entry order values*/
/*cache->sets[0].lines[i].entryorder = updateFIFO(cache->sets[0].lines, numlines);*/
/*Returns the amount of non-empty lines. arg pemptyset is either 't' or 'f' for 
  if the set is partially empty or not*/
int updateFIFO(struct line* lines, int numlines, char pemptyset){
  int i;
  for(i=0; i<numlines; i++){
    if(lines[i].validbit == 1){
      if(pemptyset == 't'){
        continue;
      }
      lines[i].entryorder--;
    }else{/*valid bit 0*/
      break;
    }
  }
  if(i == numlines){return numlines-1;}
  return i;
}

void printline(struct line* line, int linenum){
  int i;
  for(i = 0; i<linenum; i++){
    printf("line %d has entry order: %d\n", i, line[i].entryorder);
  }
  return;
}

void pwrite(struct cache* cache, unsigned long* address, int* memread, int s, int b, int t){
  unsigned long mask;
  unsigned long tagbits;
  unsigned long setindex;
  int i;
  int numlines;
  
  if(strcmp(cache->assoc, "direct") == 0){ /*Direct Mapping cache write*/
    tagbits = *address >> (b+s);
    mask = createMask(b, b+s-1);/*mask for setindex*/
    setindex = mask & *address;
    setindex = setindex >> b;

    if(cache->sets[setindex].lines[0].validbit == 1){
      if(cache->sets[setindex].lines[0].tagbits == tagbits){/*write hit*/
        return;
      }else{/*write miss*/
        cache->sets[setindex].lines[0].validbit = 1;
        cache->sets[setindex].lines[0].tagbits = tagbits;
        (*memread)++;
        return;
      }
    }else{ /*empty line, write miss*/
      cache->sets[setindex].lines[0].validbit = 1;
      cache->sets[setindex].lines[0].tagbits = tagbits;
      (*memread)++;
      return;
    }

  }else if(strcmp(cache->assoc, "assoc") == 0){ /*Fully Associative cache write*/
    tagbits = *address >> (b+s);
    numlines = (int) (cache->cache_size/cache->block_size);
    
    for(i = 0; i<numlines; i++){
      if(cache->sets[0].lines[i].validbit == 1){ /*valid bit 1*/
        if(cache->sets[0].lines[i].tagbits == tagbits){ /*write hit*/
          return;
        }else{/*tags do not match, continue checking lines*/
          continue;
        }
      }else{ /*reached empty line in set, or end of valid bits. write miss!*/
        (*memread)++;
        cache->sets[0].lines[i].entryorder = updateFIFO(cache->sets[0].lines, numlines, 't');
        cache->sets[0].lines[i].validbit = 1;
        cache->sets[0].lines[i].tagbits = tagbits;
        return;
      }
    }

    /*reaching this point means we have a write miss in a full set*/
    for(i = 0; i<numlines; i++){
      if(cache->sets[0].lines[i].entryorder == 0){
        cache->sets[0].lines[i].tagbits = tagbits;
        cache->sets[0].lines[i].entryorder = updateFIFO(cache->sets[0].lines, numlines, 'f');
        (*memread)++;
        return;
      }
    }

  }else{ /*N-way Associative cache write*/
    tagbits = *address >> (b+s);
    mask = createMask(b, b+s-1);/*mask for setindex*/
    setindex = mask & *address;
    setindex = setindex >> b;
    numlines = cache->assoc_n;
    
    /*search lines within sets[setindex]*/
    for(i = 0; i<numlines; i++){
      if(cache->sets[setindex].lines[i].validbit == 1){ /*valid bit 1*/
        if(cache->sets[setindex].lines[i].tagbits == tagbits){ /*write hit*/
          return;
        }else{/*tags do not match, continue checking lines*/
          continue;
        }
      }else{ /*reached empty line in set, or end of valid bits. write miss!*/
        (*memread)++;
        cache->sets[setindex].lines[i].entryorder = updateFIFO(cache->sets[setindex].lines, numlines, 't');
        cache->sets[setindex].lines[i].validbit = 1;
        cache->sets[setindex].lines[i].tagbits = tagbits;
        return;
      }
    }

    /*reaching this point means we have a write miss in a set of full lines*/
    for(i = 0; i<numlines; i++){
      if(cache->sets[setindex].lines[i].entryorder == 0){
        cache->sets[setindex].lines[i].tagbits = tagbits;
        cache->sets[setindex].lines[i].entryorder = updateFIFO(cache->sets[setindex].lines, numlines, 'f');
        (*memread)++;
        return;
      }
    }
    return;
  }
}

 /*write(cache, &address, &memread, &memwrite, &cachehit, &cachemiss, s, b, t);*/
void write(struct cache* cache, unsigned long* address, int* memread, int* memwrite, int* cachehit,
           int* cachemiss, int s, int b, int t, char prefetch){
  unsigned long mask;
  unsigned long tagbits;
  unsigned long setindex;
  int i;
  int numlines;
  
  if(strcmp(cache->assoc, "direct") == 0){ /*Direct Mapping cache write*/
    tagbits = *address >> (b+s);
    mask = createMask(b, b+s-1);/*mask for setindex*/
    setindex = mask & *address;
    setindex = setindex >> b;

    if(cache->sets[setindex].lines[0].validbit == 1){
      if(cache->sets[setindex].lines[0].tagbits == tagbits){/*write hit*/
        (*memwrite)++;
        (*cachehit)++;
        return;
      }else{/*write miss*/
        cache->sets[setindex].lines[0].validbit = 1;
        cache->sets[setindex].lines[0].tagbits = tagbits;
        (*memread)++;
        (*memwrite)++;
        (*cachemiss)++;
        *address = *address+(unsigned long)cache->block_size;
        if(prefetch == 'P'){pwrite(cache, address, memread, s, b, t);}
        return;
      }
    }else{ /*empty line, write miss*/
      cache->sets[setindex].lines[0].validbit = 1;
      cache->sets[setindex].lines[0].tagbits = tagbits;
      (*memread)++;
      (*memwrite)++;
      (*cachemiss)++;
      *address = *address+(unsigned long)cache->block_size;
      if(prefetch == 'P'){pwrite(cache, address, memread, s, b, t);}
      return;
    }

  }else if(strcmp(cache->assoc, "assoc") == 0){ /*Fully Associative cache write*/
    tagbits = *address >> (b+s);
    numlines = (int) (cache->cache_size/cache->block_size);
    
    for(i = 0; i<numlines; i++){
      if(cache->sets[0].lines[i].validbit == 1){ /*valid bit 1*/
        if(cache->sets[0].lines[i].tagbits == tagbits){ /*write hit*/
          (*memwrite)++;
          (*cachehit)++;
          return;
        }else{/*tags do not match, continue checking lines*/
          continue;
        }
      }else{ /*reached empty line in set, or end of valid bits. write miss!*/
        (*memread)++;
        (*memwrite)++;
        (*cachemiss)++;
        cache->sets[0].lines[i].entryorder = updateFIFO(cache->sets[0].lines, numlines, 't');
        cache->sets[0].lines[i].validbit = 1;
        cache->sets[0].lines[i].tagbits = tagbits;
        *address = *address+(unsigned long)cache->block_size;
        if(prefetch == 'P'){pwrite(cache, address, memread, s, b, t);}
        return;
      }
    }

    /*reaching this point means we have a write miss in a full set*/
    for(i = 0; i<numlines; i++){
      if(cache->sets[0].lines[i].entryorder == 0){
        cache->sets[0].lines[i].tagbits = tagbits;
        cache->sets[0].lines[i].entryorder = updateFIFO(cache->sets[0].lines, numlines, 'f');
        (*memread)++;
        (*memwrite)++;
        (*cachemiss)++;
        *address = *address+(unsigned long)cache->block_size;
        if(prefetch == 'P'){pwrite(cache, address, memread, s, b, t);}
        return;
      }
    }

  }else{ /*N-way Associative cache write*/
    tagbits = *address >> (b+s);
    mask = createMask(b, b+s-1);/*mask for setindex*/
    setindex = mask & *address;
    setindex = setindex >> b;
    numlines = cache->assoc_n;
    
    /*search lines within sets[setindex]*/
    for(i = 0; i<numlines; i++){
      if(cache->sets[setindex].lines[i].validbit == 1){ /*valid bit 1*/
        if(cache->sets[setindex].lines[i].tagbits == tagbits){ /*write hit*/
          (*memwrite)++;
          (*cachehit)++;
          return;
        }else{/*tags do not match, continue checking lines*/
          continue;
        }
      }else{ /*reached empty line in set, or end of valid bits. write miss!*/
        (*memread)++;
        (*memwrite)++;
        (*cachemiss)++;
        cache->sets[setindex].lines[i].entryorder = updateFIFO(cache->sets[setindex].lines, numlines, 't');
        cache->sets[setindex].lines[i].validbit = 1;
        cache->sets[setindex].lines[i].tagbits = tagbits;
        *address = *address+(unsigned long)cache->block_size;
        if(prefetch == 'P'){pwrite(cache, address, memread, s, b, t);}
        return;
      }
    }

    /*reaching this point means we have a write miss in a set of full lines*/
    for(i = 0; i<numlines; i++){
      if(cache->sets[setindex].lines[i].entryorder == 0){
        cache->sets[setindex].lines[i].tagbits = tagbits;
        cache->sets[setindex].lines[i].entryorder = updateFIFO(cache->sets[setindex].lines, numlines, 'f');
        (*memread)++;
        (*memwrite)++;
        (*cachemiss)++;
        *address = *address+(unsigned long)cache->block_size;
        if(prefetch == 'P'){pwrite(cache, address, memread, s, b, t);}
        return;
      }
    }
    return;
  }
}

/*read(cache, &address, &memread, &memwrite, &cachehit, &cachemiss, s, b, t);*/
void read(struct cache* cache, unsigned long* address, int* mr, int* mw, int* ch, int* cm, int s, int b, int t,
          char prefetch){
  unsigned long mask;
  unsigned long tagbits;
  unsigned long setindex;
  int i;
  int numlines;

  if(strcmp(cache->assoc, "direct") == 0){ /*Direct Mapping Cache*/
    tagbits = *address >> (b+s);
    mask = createMask(b, b+s-1);/*mask for setindex*/
    setindex = mask & *address;
    setindex = setindex >> b;

    if(cache->sets[setindex].lines[0].validbit == 1){
      if(cache->sets[setindex].lines[0].tagbits == tagbits){/*read hit*/
        (*ch)++;
        return;
      }else{/*read miss*/
        cache->sets[setindex].lines[0].validbit = 1;
        cache->sets[setindex].lines[0].tagbits = tagbits;
        (*mr)++;
        (*cm)++;
        *address = *address+(unsigned long)cache->block_size;
        if(prefetch == 'P'){pwrite(cache, address, mr, s, b, t);}
        return;
      }
    }else{ /*empty line, read miss*/
      cache->sets[setindex].lines[0].validbit = 1;
      cache->sets[setindex].lines[0].tagbits = tagbits;
      (*mr)++;
      (*cm)++;
      *address = *address+(unsigned long)cache->block_size;
      if(prefetch == 'P'){pwrite(cache, address, mr, s, b, t);}
      return;
    }

  }else if(strcmp(cache->assoc, "assoc") == 0){/*Fully Associative Cache*/
    tagbits = *address >> (b+s);
    numlines = (cache->cache_size/cache->block_size);

    for(i = 0; i<numlines; i++){
      if(cache->sets[0].lines[i].validbit == 1){ /*valid bit 1*/
        if(cache->sets[0].lines[i].tagbits == tagbits){ /*read hit*/
          (*ch)++;
          return;
        }else{/*tags do not match, continue checking lines*/
          continue;
        }
      }else{ /*reached empty line in set, or end of valid bits. read miss!*/
        (*mr)++;
        (*cm)++;
        cache->sets[0].lines[i].entryorder = updateFIFO(cache->sets[0].lines, numlines, 't');
        cache->sets[0].lines[i].validbit = 1;
        cache->sets[0].lines[i].tagbits = tagbits;
        *address = *address+(unsigned long)cache->block_size;
        if(prefetch == 'P'){pwrite(cache, address, mr, s, b, t);}
        return;
      }
    }

    /*reaching this point means we have a read miss in a set of full lines*/
    for(i = 0; i<numlines; i++){
      if(cache->sets[0].lines[i].entryorder == 0){
        cache->sets[0].lines[i].tagbits = tagbits;
        cache->sets[0].lines[i].entryorder = updateFIFO(cache->sets[0].lines, numlines, 'f');
        (*mr)++;
        (*cm)++;
        *address = *address+(unsigned long)cache->block_size;
        if(prefetch == 'P'){pwrite(cache, address, mr, s, b, t);}
        return;
      }
    }
    return;

  }else{/*N-way Associative Cache*/
    tagbits = *address >> (b+s);
    mask = createMask(b, b+s-1);/*mask for setindex*/
    setindex = mask & *address;
    setindex = setindex >> b;
    numlines = cache->assoc_n;

    /*search lines within sets[setindex]*/
    for(i = 0; i<numlines; i++){
      if(cache->sets[setindex].lines[i].validbit == 1){ /*valid bit 1*/
        if(cache->sets[setindex].lines[i].tagbits == tagbits){ /*read hit*/
          (*ch)++;
          return;
        }else{/*tags do not match, continue checking lines*/
          continue;
        }
      }else{ /*reached empty line in set, or end of valid bits. read miss!*/
        cache->sets[setindex].lines[i].entryorder = updateFIFO(cache->sets[setindex].lines, numlines, 't');
        cache->sets[setindex].lines[i].validbit = 1;
        cache->sets[setindex].lines[i].tagbits = tagbits;
        (*mr)++;
        (*cm)++;
        *address = *address+(unsigned long)cache->block_size;
        if(prefetch == 'P'){pwrite(cache, address, mr, s, b, t);}
        return;
      }
    }
    
    /*reaching this point means we have a read miss in a set of full lines*/
    for(i = 0; i<numlines; i++){
      if(cache->sets[setindex].lines[i].entryorder == 0){
        cache->sets[setindex].lines[i].tagbits = tagbits;
        cache->sets[setindex].lines[i].entryorder = updateFIFO(cache->sets[setindex].lines, numlines, 'f');
        (*mr)++;
        (*cm)++;
        *address = *address+(unsigned long)cache->block_size;
        if(prefetch == 'P'){pwrite(cache, address, mr, s, b, t);}
        return;
      }
    }
  }
  return;
}

int main(int argc, char** argv){
  /*arg1=<cache size>, arg2=<associativity>, arg3=<cache policy>, arg4=<block size>, arg5=<trace file>*/
  char op;
  char* assoc = argv[2];
  char* p = assoc;
  char* cache_pol = argv[3];
  char* trace_file = argv[5];
  int cache_size = strtol(argv[1], NULL, 10);
  int block_size = strtol(argv[4], NULL, 10);
  int assoc_n = 0;
  int memread = 0;
  int memwrite = 0;
  int cachehit = 0;
  int cachemiss = 0;
  double s; /*# of set index bits*/
  double b; /*# of block offset bits*/
  double t; /*# of tag index bits*/
  unsigned long address = 0;
  float i = 0.0;
  struct cache* cache;
  FILE* fp = NULL;

  /*Setting up file for parsing*/
  fp = fopen(trace_file, "r");

  /*Parse arg2 for Associativity number*/
  while(*p){
    if(isdigit(*p)){
      assoc_n = strtol(p, NULL, 10);
      break;
    } else{
      p++;
    }
  }
  
  /*Checking for correctly formatted input*/
  /*Checking number of args*/
  if(argc != 6){
    printf("Incorrect input format: You inputted %d arguments, this program requires 5.\n", argc-1);
    exit(0);
  }
  
  /*Verify cache_policy is correctly formatted*/
  if(strcmp(cache_pol, "lru") != 0 && strcmp(cache_pol, "fifo") != 0){
    printf("Incorrect input: cache policy must be \"lru\" or \"fifo\" (arg4).\n");
    exit(0);
  }

  /*Verify assoc is correctly formatted*/
  if(strcmp(assoc, "direct") != 0 && strcmp(assoc, "assoc") != 0 
     && ((assoc_n == -1 && strstr(assoc, "assoc:") != NULL) || (assoc_n != -1 && strstr(assoc, "assoc:") == NULL))){
    printf("Incorecct input: associativity must be \"direct\", \"assoc\", or \"assoc:n\", where n is power of 2 (arg2)\n");
    exit(0);
  }

  /*Verify cache_size, block_size, and assoc_n are powers of 2*/
  if(cache_size == 1 || assoc_n == 1){
    printf("Incorrect input: cache size and/or associativity cannot be 1!\n");
    exit(0);
  }
  
  i = (float)cache_size;
  while(i > 1){
    i = i/2;
  }
  if(i != 1.0){
    printf("Incorrect input: cache size must be a power of 2 (arg1)!\n");
    exit(0);
  }
  
  i = (float)block_size;
  while(i > 1){
    i = i/2;
  }
  if(i != 1.0){
    printf("Incorrect input: block size must be a power of 2 (arg5)!\n");
    exit(0);
  }

  if(assoc_n != 0){
    i = (float)assoc_n;
    while(i > 1){
      i = i/2;
    }
    if(i != 1.0){
      printf("Incorrect input: associativity must be a power of 2 (arg2)!\n");
      exit(0);
    }
  }
  
  /*Verify Cache dimensions are possible*/
  if(assoc_n != 0){
    if(cache_size < assoc_n * block_size){
      printf("Incorrect input: impossible cache dimensions!\n");
      exit(0);
    }
  }

  /*Initialize cache with given arguments*/
  cache = InitializeCache(cache_size, assoc_n, assoc, cache_pol, block_size, &s, &b, &t);

  /*Scan file and perform ops on addresses in cache. Update values. No prefetcher*/
  while(fscanf(fp, "%*x: %c %lx", &op, &address) == 2){
    if(op == 'W'){
      write(cache, &address, &memread, &memwrite, &cachehit, &cachemiss, s, b, t, 'N');
    }else if(op == 'R'){
      read(cache, &address, &memread, &memwrite, &cachehit, &cachemiss, s, b, t, 'N');
    }
  }
  /*Print values out*/
  printf("no-prefetch\n");
  printf("Memory reads: %d\n", memread);
  printf("Memory writes: %d\n", memwrite);
  printf("Cache hits: %d\n", cachehit);
  printf("Cache misses: %d\n", cachemiss);

  /*Reset values*/
  memread = 0;
  memwrite = 0;
  cachehit = 0;
  cachemiss = 0;

  /*new cache*/
  cache = InitializeCache(cache_size, assoc_n, assoc, cache_pol, block_size, &s, &b, &t);

  fseek(fp, 0, SEEK_SET); /*Reset fp stream pointer*/
  /*Scan file and perform ops on addresses in cache. Update values. WITH prefetcher*/
  while(fscanf(fp, "%*x: %c %lx", &op, &address) == 2){
    if(op == 'W'){
      write(cache, &address, &memread, &memwrite, &cachehit, &cachemiss, s, b, t, 'P');
    }else if(op == 'R'){
      read(cache, &address, &memread, &memwrite, &cachehit, &cachemiss, s, b, t, 'P');
    }
  }
  /*Print values out*/
  printf("with-prefetch\n");
  printf("Memory reads: %d\n", memread);
  printf("Memory writes: %d\n", memwrite);
  printf("Cache hits: %d\n", cachehit);
  printf("Cache misses: %d\n", cachemiss);

  return 0;
}
