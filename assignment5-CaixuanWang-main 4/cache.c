#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

// Set cache initializtion variable.
static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;
int init_cache=0;
int wroten_num=0; // Cache's existing block number.

// Create the cache as array with predefined space.
int cache_create(int num) {
  if( (num<2) || (num>4096) || (init_cache!=0) ){
    return -1;
  }
  cache=calloc(num,sizeof(cache_entry_t));
  cache_size=num;
  init_cache=1;
  return 1;
}

// Shut down cache by free cache and re-initialize cache set-up variable.
int cache_destroy(void) {
  if(init_cache!=1){
    return -1;
  }
  free(cache);
  cache=NULL;
  cache_size=0;
  init_cache=0;
  return 1;
}

/* Search by disk and block id in cache and copy it to buf.
   Also update its time and count its successful hit time. */
int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
   if( (init_cache!=1) || (disk_num<0) || (disk_num>15) || (block_num<0) || (block_num>255) || (wroten_num==0) || (buf==NULL) ){
    return -1;
  }
  num_queries+=1;
  for(int i=0;i<cache_size;i++){
    if(disk_num==cache[i].disk_num && block_num==cache[i].block_num && cache[i].block != NULL){
      memcpy(buf,cache[i].block,JBOD_BLOCK_SIZE);
      num_hits+=1;
      clock+=1;
      cache[i].access_time=clock;
      return 1;
    }
  }
  return -1;
}

// Update changes of cache in mdadm.
void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  for(int i=0;i<cache_size;i++){
    if(disk_num==cache[i].disk_num && block_num==cache[i].block_num && buf != NULL){
      memcpy(cache[i].block,buf,JBOD_BLOCK_SIZE);
      clock+=1;
      cache[i].access_time=clock;
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  // Check the false case.
  if( (init_cache!=1) || (buf==NULL) || (disk_num<0) || (disk_num>15) || (block_num<0) || (block_num>255) ){
    return -1;
  }
  for(int i=0;i<cache_size;i++){
    if(disk_num==cache[i].disk_num && block_num==cache[i].block_num && buf != NULL && cache[i].access_time>0){
      return -1;
    }
  }
  /*
   * First, check if cache is full.
   * If not full, insert the block.
   * If it's full, use the clock time updates before to replace the oldest one with new block.
   */
  if(wroten_num==cache_size){
    int old_int=0;
    int old_time=0;
    for(int i=0;i<cache_size;i++){
      if(cache[i].access_time>old_time){
        old_time=cache[i].access_time;
        old_int=i;
      }
    }
    clock+=1;
    cache[old_int].block_num=block_num;
    cache[old_int].disk_num=disk_num;
    cache[old_int].access_time=clock;
    memcpy(cache[old_int].block,buf,JBOD_BLOCK_SIZE);
    return 1;
  }
  else{
    clock+=1;
    cache[wroten_num].block_num=block_num;
    cache[wroten_num].disk_num=disk_num;
    cache[wroten_num].access_time=clock;
    memcpy(cache[wroten_num].block,buf,JBOD_BLOCK_SIZE);
    wroten_num+=1;
    return 1;
  }
  return -1;
}

bool cache_enabled(void) {
  if(init_cache==1){
    return true;
  }
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}