#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"
int mount_time=0;
uint32_t encode_operation(uint32_t com, uint32_t did, uint32_t bid){
  uint32_t op = 0x0, res=0x0,tembid, temres, temdid, temcom;
  tembid = (bid & 0xff);
  temres = (res & 0xff) << 8;
  temdid = (did & 0xff) << 22;
  temcom = (com & 0xff) << 26;
  op = tembid | temres | temdid | temcom;
  return(op);
}

void translate_address(uint32_t linear_address, int *did, int *bid, int *offset){
  *did=linear_address/JBOD_DISK_SIZE;
  *bid=(linear_address%JBOD_DISK_SIZE)/JBOD_BLOCK_SIZE;
  *offset=(linear_address%JBOD_DISK_SIZE)%JBOD_BLOCK_SIZE;
}

void seek(int did,int bid){
  uint32_t op=encode_operation(JBOD_SEEK_TO_DISK,did,0);
  uint8_t* block=NULL;
  jbod_operation(op,block);
  op=encode_operation(JBOD_SEEK_TO_BLOCK,0,bid);
  jbod_operation(op,block);
}

int mdadm_mount(void){
  uint32_t op=encode_operation(JBOD_MOUNT,0,0);
  uint8_t* block=NULL;
  if(mount_time!=0){
    return(-1);
  }
  int result=jbod_operation(op,block);
  if(result==0){
    mount_time=1;
    return(1);
  }
  return(-1);
}

int mdadm_unmount(void){
  uint32_t op=encode_operation(JBOD_UNMOUNT,0,0);
  uint8_t* block=NULL;
  if(mount_time!=1){
    return(-1);
  }
  int result=jbod_operation(op,block);
  if(result==0){
    mount_time=0;
    return(1);
  }
  return (-1);
}

void check_block(int len,int bid,int did,int offset,uint32_t op,uint8_t buf6[]){
  int block_number=1+(len+offset)/256;
  if(block_number==1){
    uint8_t buf1[256];
    seek(did,bid);
    jbod_operation(op,buf1);
    for(int i=offset;i<offset+len;i++){
      buf6[i-offset]=buf1[i];
    }
  }
  if(block_number==2){
    uint8_t buf1[256],buf2[256];
    seek(did,bid);
    jbod_operation(op,buf1);
    seek(did,bid+1);
    jbod_operation(op,buf2);
    for(int i=offset;i<256;i++){
      buf6[i-offset]=buf1[i];
    }
    for(int i=0;i<len-256+offset;i++){
      buf6[256-offset+i]=buf2[i];
    }
  }
  if(block_number==3){
    uint8_t buf1[256],buf2[256],buf3[256];
    seek(did,bid);
    jbod_operation(op,buf1);
    seek(did,bid+1);
    jbod_operation(op,buf2);
    seek(did,bid+2);
    jbod_operation(op,buf3);
    for(int i=offset;i<256;i++){
      buf6[i-offset]=buf1[i];
    }
    for(int i=0;i<256;i++){
      buf6[256-offset+i]=buf2[i];
    }
    for(int i=0;i<len-512+offset;i++){
      buf6[512-offset+i]=buf3[i];
    }
  }
  if(block_number==4){
    uint8_t buf1[256],buf2[256],buf3[256],buf4[256];
    seek(did,bid);
    jbod_operation(op,buf1);
    seek(did,bid+1);
    jbod_operation(op,buf2);
    seek(did,bid+2);
    jbod_operation(op,buf3);
    seek(did,bid+3);
    jbod_operation(op,buf4);
    for(int i=offset;i<256;i++){
      buf6[i-offset]=buf1[i];
    }
    for(int i=0;i<256;i++){
      buf6[256-offset+i]=buf2[i];
    }
    for(int i=0;i<256;i++){
      buf6[512-offset+i]=buf3[i];
    }
    for(int i=0;i<len-768+offset;i++){
      buf6[768-offset+i]=buf4[i];
    }
  }
  if(block_number==5){
    uint8_t buf1[256],buf2[256],buf3[256],buf4[256],buf5[256];
    seek(did,bid);
    jbod_operation(op,buf1);
    seek(did,bid+1);
    jbod_operation(op,buf2);
    seek(did,bid+2);
    jbod_operation(op,buf3);
    seek(did,bid+3);
    jbod_operation(op,buf4);
    seek(did,bid+4);
    jbod_operation(op,buf5);
    for(int i=offset;i<256;i++){
      buf6[i-offset]=buf1[i];
    }
    for(int i=0;i<256;i++){
      buf6[256-offset+i]=buf2[i];
    }
    for(int i=0;i<256;i++){
      buf6[512-offset+i]=buf3[i];
    }
    for(int i=0;i<256;i++){
      buf6[768-offset+i]=buf4[i];
    }
    for(int i=0;i<len-1024+offset;i++){
      buf6[1024-offset+i]=buf5[i];
    }
  }
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf)
{
  uint32_t op=encode_operation(JBOD_READ_BLOCK,0,0);
  if(mount_time!=1 | len>1024 | len<0| addr+len>JBOD_DISK_SIZE*16-1){
    return(-1);
  }
  if(len==0 && buf==NULL){
    buf=NULL;
    return len;
  }
  if(buf==NULL && len!=0){
    return(-1);
  }
  int bid,did,offset,bidafter,didafter,offsetafter;
  translate_address(addr,&did,&bid,&offset);
  translate_address(addr+len,&didafter,&bidafter,&offsetafter);
  if(len<=256-offset){
    uint8_t buf1[256],buf2[len];
    seek(did,bid);
    jbod_operation(op,buf1);
    for(int i=offset;i<offset+len;i++){
      buf2[i-offset]=buf1[i];
    }
    memcpy(buf,&buf2,len);
    return len;
  }
  if(didafter==did){
    uint8_t buf6[len];
    check_block(len,bid,did,offset,op,buf6);
    memcpy(buf,&buf6,len);
    return len;
  }
  else{
    int first_disk_size=(did+1)*JBOD_DISK_SIZE-addr,second_disk_size=len-first_disk_size;
    uint8_t buf6[first_disk_size],buf7[second_disk_size],buf8[len];
    check_block(first_disk_size,bid,did,offset,op,buf6);
    check_block(second_disk_size,0,did+1,0,op,buf7);
    for(int i=0;i<first_disk_size;i++){
      buf8[i]=buf6[i];
    }
    for(int i=first_disk_size;i<len;i++){
      buf8[i]=buf7[i-first_disk_size];
    }
    memcpy(buf,&buf8,len);
    return len;
  }
  return len;
}