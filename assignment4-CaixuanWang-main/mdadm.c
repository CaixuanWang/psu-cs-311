#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"
#include "cache.h"
#include <stdlib.h>

int is_mount=0;

// This function takes command, diskid, blockid as input, merges them to op as output.
uint32_t encode_operation(uint32_t com, uint32_t did, uint32_t bid){
  uint32_t op = 0x0, res=0x0,tbid, tres, tdid, tcom;
  tbid = (bid & 0xff);
  tres = (res & 0xff) << 8;
  tdid = (did & 0xff) << 22;
  tcom = (com & 0xff) << 26;
  op = tbid | tres | tdid | tcom;
  return(op);
}

/* This function translates linear_address into disk, block and offset.
   Then it saves them in the input pointer. */
void translate_address(uint32_t linear_addr, int *did, int *bid, int *offset){
  *did=linear_addr/JBOD_DISK_SIZE;
  *bid=(linear_addr%JBOD_DISK_SIZE)/JBOD_BLOCK_SIZE;
  *offset=(linear_addr%JBOD_DISK_SIZE)%JBOD_BLOCK_SIZE;
}

// This function seeks to disk first, then seeks to block.
void seek(int did,int bid){
  uint32_t op=encode_operation(JBOD_SEEK_TO_DISK,did,0);
  uint8_t* block=NULL;
  jbod_operation(op,block);
  op=encode_operation(JBOD_SEEK_TO_BLOCK,0,bid);
  jbod_operation(op,block);
}

// This function mounts when mdadm isn't mounted before. 
int mdadm_mount(void){
  uint32_t op=encode_operation(JBOD_MOUNT,0,0);
  uint8_t* block=NULL;
  if(is_mount!=0){
    return(-1);
  }
  int result=jbod_operation(op,block);
  if(result==0){
    is_mount=1;
    return(1);
  }
  return(-1);
}

// This function unmounts when mdadm is mounted.
int mdadm_unmount(void){
  uint32_t op=encode_operation(JBOD_UNMOUNT,0,0);
  uint8_t* block=NULL;
  if(is_mount!=1){
    return(-1);
  }
  int result=jbod_operation(op,block);
  if(result==0){
    is_mount=0;
    return(1);
  }
  return (-1);
}

/* This function tries to find the block is in cache or not.
   If it's in cache, use cache. Otherwise, use jbod_operation. */
void decide_cache_or_op(int did,int bid,uint32_t op,uint8_t buf[JBOD_BLOCK_SIZE]){
  if( (cache_enabled()==true) && (cache_lookup(did,bid,buf)==1) ){
    cache_lookup(did,bid,buf);
  }
  else{
    seek(did,bid);
    jbod_operation(op,buf);
    decide_update_or_insert(did,bid,buf);
  }
}

/* This function tries to find the block is in cache or not.
   If it's in cache, update. Otherwise, insert. */
void decide_update_or_insert(int did,int bid,uint8_t buf[JBOD_BLOCK_SIZE]){
  uint8_t buf_1[JBOD_BLOCK_SIZE];
  if(cache_enabled()==true){
    if(cache_lookup(did,bid,buf_1)==-1){
      cache_insert(did,bid,buf);
    }
    else{
      cache_update(did,bid,buf);
    }
  }
}

/* 
 * This function tries to find how many blocks will be needed to read.
 * Then it gets all blocks by decide_cache_or_op function.
 * Finally, it merges them to the buf_merge.
 */
void read_check_block(int len,int bid,int did,int offset,uint32_t op,uint8_t buf_merge[]){
  int blocknum=1+(len+offset)/JBOD_BLOCK_SIZE;
  if(blocknum==1){
    uint8_t buf1[JBOD_BLOCK_SIZE];
    decide_cache_or_op(did,bid,op,buf1);
    for(int i=offset;i<offset+len;i++)
      buf_merge[i-offset]=buf1[i];
  }
  else if(blocknum==2){
    uint8_t buf1[JBOD_BLOCK_SIZE],buf2[JBOD_BLOCK_SIZE];
    decide_cache_or_op(did,bid,op,buf1);
    decide_cache_or_op(did,bid+1,op,buf2);
    for(int i=offset;i<JBOD_BLOCK_SIZE;i++)
      buf_merge[i-offset]=buf1[i];
    for(int i=0;i<len-JBOD_BLOCK_SIZE+offset;i++)
      buf_merge[JBOD_BLOCK_SIZE-offset+i]=buf2[i];
  }
  else if(blocknum==3){
    uint8_t buf1[JBOD_BLOCK_SIZE],buf2[JBOD_BLOCK_SIZE],buf3[JBOD_BLOCK_SIZE];
    decide_cache_or_op(did,bid,op,buf1);
    decide_cache_or_op(did,bid+1,op,buf2);
    decide_cache_or_op(did,bid+2,op,buf3);
    for(int i=offset;i<JBOD_BLOCK_SIZE;i++)
      buf_merge[i-offset]=buf1[i];
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf_merge[JBOD_BLOCK_SIZE-offset+i]=buf2[i];
    for(int i=0;i<len-JBOD_BLOCK_SIZE*2+offset;i++)
      buf_merge[JBOD_BLOCK_SIZE*2-offset+i]=buf3[i];
  }
  else if(blocknum==4){
    uint8_t buf1[JBOD_BLOCK_SIZE],buf2[JBOD_BLOCK_SIZE],buf3[JBOD_BLOCK_SIZE],buf4[JBOD_BLOCK_SIZE];
    decide_cache_or_op(did,bid,op,buf1);
    decide_cache_or_op(did,bid+1,op,buf2);
    decide_cache_or_op(did,bid+2,op,buf3);
    decide_cache_or_op(did,bid+3,op,buf4);
    for(int i=offset;i<JBOD_BLOCK_SIZE;i++)
      buf_merge[i-offset]=buf1[i];
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf_merge[JBOD_BLOCK_SIZE-offset+i]=buf2[i];
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf_merge[JBOD_BLOCK_SIZE*2-offset+i]=buf3[i];
    for(int i=0;i<len-JBOD_BLOCK_SIZE*3+offset;i++)
      buf_merge[JBOD_BLOCK_SIZE*3-offset+i]=buf4[i];
  }
  else if(blocknum==5){
    uint8_t buf1[JBOD_BLOCK_SIZE],buf2[JBOD_BLOCK_SIZE],buf3[JBOD_BLOCK_SIZE],buf4[JBOD_BLOCK_SIZE],buf5[JBOD_BLOCK_SIZE];
    decide_cache_or_op(did,bid,op,buf1);
    decide_cache_or_op(did,bid+1,op,buf2);
    decide_cache_or_op(did,bid+2,op,buf3);
    decide_cache_or_op(did,bid+3,op,buf4);
    decide_cache_or_op(did,bid+4,op,buf5);
    for(int i=offset;i<JBOD_BLOCK_SIZE;i++)
      buf_merge[i-offset]=buf1[i];
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf_merge[JBOD_BLOCK_SIZE-offset+i]=buf2[i];
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf_merge[JBOD_BLOCK_SIZE*2-offset+i]=buf3[i];
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf_merge[JBOD_BLOCK_SIZE*3-offset+i]=buf4[i];
    for(int i=0;i<len-JBOD_BLOCK_SIZE*4+offset;i++)
      buf_merge[JBOD_BLOCK_SIZE*4-offset+i]=buf5[i];
  }
}

/* 
 * This function tries to find how many blocks will be needed to write.
 * Then it gets all blocks by decide_cache_or_op function.
 * Finally, it merges them to the buf_merge.
 */
void write_check_block(int len,int bid, int did, int offset, uint32_t op, uint32_t op_r,uint8_t buf_merge[]){
  int blocknum=1+(len+offset)/JBOD_BLOCK_SIZE;
  if(blocknum==1){
    uint8_t buf1[JBOD_BLOCK_SIZE];
    decide_cache_or_op(did,bid,op_r,buf1);
    seek(did,bid);
    for(int i=offset;i<offset+len;i++)
      buf1[i]=buf_merge[i-offset];
    jbod_operation(op,buf1);
    decide_update_or_insert(did,bid,buf1);
  }
  else if(blocknum==2){
    uint8_t buf1[JBOD_BLOCK_SIZE],buf2[JBOD_BLOCK_SIZE];
    decide_cache_or_op(did,bid,op_r,buf1);
    decide_cache_or_op(did,bid+1,op_r,buf2);
    for(int i=offset;i<JBOD_BLOCK_SIZE;i++)
      buf1[i]=buf_merge[i-offset];
    seek(did,bid);
    jbod_operation(op,buf1);
    for(int i=0;i<len-JBOD_BLOCK_SIZE+offset;i++)
      buf2[i]=buf_merge[JBOD_BLOCK_SIZE-offset+i];
    seek(did,bid+1);
    jbod_operation(op,buf2);
    decide_update_or_insert(did,bid,buf1);
    decide_update_or_insert(did,bid+1,buf2);
  }
  else if(blocknum==3){
    uint8_t buf1[JBOD_BLOCK_SIZE],buf2[JBOD_BLOCK_SIZE],buf3[JBOD_BLOCK_SIZE];
    decide_cache_or_op(did,bid,op_r,buf1);
    decide_cache_or_op(did,bid+1,op_r,buf2);
    decide_cache_or_op(did,bid+2,op_r,buf3);
    for(int i=offset;i<JBOD_BLOCK_SIZE;i++)
      buf1[i]=buf_merge[i-offset];
    seek(did,bid);
    jbod_operation(op,buf1);
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf2[i]=buf_merge[i-offset+JBOD_BLOCK_SIZE];
    seek(did,bid+1);
    jbod_operation(op,buf2);
    for(int i=0;i<len-JBOD_BLOCK_SIZE*2+offset;i++)
      buf3[i]=buf_merge[JBOD_BLOCK_SIZE*2-offset+i];
    seek(did,bid+2);
    jbod_operation(op,buf3);
    decide_update_or_insert(did,bid,buf1);
    decide_update_or_insert(did,bid+1,buf2);
    decide_update_or_insert(did,bid+2,buf3);
  }
  else if(blocknum==4){
    uint8_t buf1[JBOD_BLOCK_SIZE],buf2[JBOD_BLOCK_SIZE],buf3[JBOD_BLOCK_SIZE],buf4[JBOD_BLOCK_SIZE];
    decide_cache_or_op(did,bid,op_r,buf1);
    decide_cache_or_op(did,bid+1,op_r,buf2);
    decide_cache_or_op(did,bid+2,op_r,buf3);
    decide_cache_or_op(did,bid+3,op_r,buf4);
    for(int i=offset;i<JBOD_BLOCK_SIZE;i++)
      buf1[i]=buf_merge[i-offset];
    seek(did,bid);
    jbod_operation(op,buf1);
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf2[i]=buf_merge[i-offset+JBOD_BLOCK_SIZE];
    seek(did,bid+1);
    jbod_operation(op,buf2);
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf3[i]=buf_merge[i-offset+JBOD_BLOCK_SIZE*2];
    seek(did,bid+2);
    jbod_operation(op,buf3);
    for(int i=0;i<len-JBOD_BLOCK_SIZE*3+offset;i++)
      buf4[i]=buf_merge[JBOD_BLOCK_SIZE*3-offset+i];
    seek(did,bid+3);
    jbod_operation(op,buf4);
    decide_update_or_insert(did,bid,buf1);
    decide_update_or_insert(did,bid+1,buf2);
    decide_update_or_insert(did,bid+2,buf3);
    decide_update_or_insert(did,bid+3,buf4);
  }
  else if(blocknum==5){
    uint8_t buf1[JBOD_BLOCK_SIZE],buf2[JBOD_BLOCK_SIZE],buf3[JBOD_BLOCK_SIZE],buf4[JBOD_BLOCK_SIZE],buf5[JBOD_BLOCK_SIZE];
    decide_cache_or_op(did,bid,op_r,buf1);
    decide_cache_or_op(did,bid+1,op_r,buf2);
    decide_cache_or_op(did,bid+2,op_r,buf3);
    decide_cache_or_op(did,bid+3,op_r,buf4);
    decide_cache_or_op(did,bid+4,op_r,buf5);
    for(int i=offset;i<JBOD_BLOCK_SIZE;i++)
      buf1[i]=buf_merge[i-offset];
    seek(did,bid);
    jbod_operation(op,buf1);
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf2[i]=buf_merge[i-offset+JBOD_BLOCK_SIZE];
    seek(did,bid+1);
    jbod_operation(op,buf2);
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf3[i]=buf_merge[i-offset+JBOD_BLOCK_SIZE*2];
    seek(did,bid+2);
    jbod_operation(op,buf3);
    for(int i=0;i<JBOD_BLOCK_SIZE;i++)
      buf4[i]=buf_merge[i-offset+JBOD_BLOCK_SIZE*3];
    seek(did,bid+3);
    jbod_operation(op,buf4);
    for(int i=0;i<len-JBOD_BLOCK_SIZE*4+offset;i++)
      buf5[i]=buf_merge[JBOD_BLOCK_SIZE*4-offset+i];
    seek(did,bid+4);
    jbod_operation(op,buf5);
    decide_update_or_insert(did,bid,buf1);
    decide_update_or_insert(did,bid+1,buf2);
    decide_update_or_insert(did,bid+2,buf3);
    decide_update_or_insert(did,bid+3,buf4);
    decide_update_or_insert(did,bid+4,buf5);
  }
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf)
{
  // Check false case.
  if( (is_mount!=1) || (len>JBOD_BLOCK_SIZE*4) || (len<0) || (addr+len>JBOD_DISK_SIZE*16) ){
    return(-1);
  }
  if( (len==0) && (buf==NULL) ){
    buf=NULL;
    return len;
  }
  if( (buf==NULL) && (len!=0) ){
    return(-1);
  }
  // Set up all I/O position.
  uint32_t op=encode_operation(JBOD_READ_BLOCK,0,0);
  int bid,did,offset,bidafter,didafter,offsetafter;
  translate_address(addr,&did,&bid,&offset);
  translate_address(addr+len,&didafter,&bidafter,&offsetafter);
  /* 
   * Check if all blocks that need to be read are cross disk or not.
   * Then use helper function to check if they are cross blocks.
   * Finally, read them.
   */
  if(didafter==did){
    uint8_t buf_merge[len];
    read_check_block(len,bid,did,offset,op,buf_merge);
    memcpy(buf,&buf_merge,len);
    return len;
  }
  else{
    int first_disk_size=(did+1)*JBOD_DISK_SIZE-addr,second_disk_size=len-first_disk_size;
    uint8_t buf_merge[first_disk_size],buf7[second_disk_size],buf8[len];
    read_check_block(first_disk_size,bid,did,offset,op,buf_merge);
    read_check_block(second_disk_size,0,did+1,0,op,buf7);
    for(int i=0;i<first_disk_size;i++)
      buf8[i]=buf_merge[i];
    for(int i=first_disk_size;i<len;i++)
      buf8[i]=buf7[i-first_disk_size];
    memcpy(buf,&buf8,len);
    return len;
  }
  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  // Check false case.
  if( (is_mount!=1) || (len>JBOD_BLOCK_SIZE*4) || (len<0) || (addr+len>JBOD_DISK_SIZE*16) ){
    return(-1);
  }
  if( (len==0) && (buf==NULL) ){
    return len;
  }
  if( (buf==NULL) && (len!=0) ){
    return(-1);
  }
  // Set up all I/O position.
  uint32_t op=encode_operation(JBOD_WRITE_BLOCK,0,0);
  uint32_t op_r=encode_operation(JBOD_READ_BLOCK,0,0);
  int bid,did,offset,bidafter,didafter,offsetafter;
  translate_address(addr,&did,&bid,&offset);
  translate_address(addr+len,&didafter,&bidafter,&offsetafter);
  /* 
   * Check if all blocks that need to be read are cross disk or not.
   * Then use helper function to check if they are cross blocks.
   * Finally, write them.
   */
  if(didafter==did){
    uint8_t buf_merge[len];
    memcpy(buf_merge, buf,len);
    write_check_block(len,bid,did,offset,op,op_r,buf_merge);
    return len;
  }
  else{
    int first_disk_size=(did+1)*JBOD_DISK_SIZE-addr,second_disk_size=len-first_disk_size;
    uint8_t buf_merge[first_disk_size],buf7[second_disk_size],buf8[len];
    memcpy(buf8,buf,len);
    for(int i=0;i<first_disk_size;i++)
      buf_merge[i]=buf8[i];
    for(int i=first_disk_size;i<len;i++)
      buf7[i-first_disk_size]=buf8[i];
    write_check_block(first_disk_size,bid,did,offset,op,op_r,buf_merge);
    write_check_block(second_disk_size,0,did+1,0,op,op_r,buf7);
    return len;
  }
  return len;
}