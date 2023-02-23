#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

uint32_t encode_op(uint32_t com, uint32_t did, uint32_t bid){
  uint32_t op = 0x0, res=0x0,tbid, tres, tdid, tcom;
  tbid = (bid & 0xff);
  tres = (res & 0xff) << 8;
  tdid = (did & 0xff) << 22;
  tcom = (com & 0xff) << 26;
  op = tbid | tres | tdid | tcom;
  return(op);
}
/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
  int num_read=0;
  while(num_read < len){
    int n=read(fd, &buf[num_read], len-num_read);
    if(n<0){
      return false;
    }
    num_read+=n;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
  int num_write=0;
  while(num_write < len){
    int n=write(fd, &buf[num_write], len-num_write);
    if(n<0){
      return false;
    }
    num_write+=n;
  }
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  uint16_t len;
  uint16_t full_len=HEADER_LEN+JBOD_BLOCK_SIZE;
  int offset=0;
  uint8_t header[HEADER_LEN];
  if(!(nread(fd,HEADER_LEN,header))){
    return false;
  }
  memcpy(&len,header+offset,sizeof(len));
  offset+=sizeof(len);
  memcpy(op,header+offset,sizeof(*op));
  offset+=sizeof(*op);
  memcpy(ret,header+offset,sizeof(*ret));
  len=ntohs(len);
  *op=ntohl(*op);
  *ret=ntohs(*ret);
  if(len==HEADER_LEN){
    block=NULL;
    return true;
  }
  else if(len==full_len){
    offset+=sizeof(*ret);
    if(!(nread(fd,JBOD_BLOCK_SIZE,block))){
      return false;
    }
    // memcpy(block,fullp,JBOD_BLOCK_SIZE);
    return true;
  }
  else{
    return false;
  }
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint16_t lenh=HEADER_LEN, lenf=HEADER_LEN+JBOD_BLOCK_SIZE, ret=0;
  uint32_t opp=op;
  uint8_t header[HEADER_LEN], fullp[lenf];
  opp=htonl(opp);
  ret=htons(ret);
  lenh=htons(lenh);
  lenf=htons(lenf);
  int offset=0;
  memcpy(header+offset,&lenh,sizeof(lenh));
  offset+=sizeof(lenh);
  memcpy(header+offset,&opp,sizeof(opp));
  offset+=sizeof(opp);
  memcpy(header+offset,&ret,sizeof(ret));
  offset=0;
  memcpy(fullp+offset,&lenf,sizeof(lenf));
  offset+=sizeof(lenf);
  memcpy(fullp+offset,&opp,sizeof(opp));
  offset+=sizeof(opp);
  memcpy(fullp+offset,&ret,sizeof(ret));
  offset+=sizeof(ret);
  if((op!=encode_op(JBOD_WRITE_BLOCK,0,0)) && (nwrite(sd,HEADER_LEN,header) )){
    return true;
  }
  else if(op==encode_op(JBOD_WRITE_BLOCK,0,0)){
    memcpy(fullp+offset,block,JBOD_BLOCK_SIZE);
    if(nwrite(sd,HEADER_LEN+JBOD_BLOCK_SIZE,fullp)==true){
      return true;
    }
  }
  return false;
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in caddr;
  caddr.sin_family=AF_INET;
  caddr.sin_port=htons(port);
  cli_sd=socket(AF_INET,SOCK_STREAM,0);
  if(inet_aton(ip,&caddr.sin_addr) == 0 || cli_sd == -1){
    return false;
  }
  if (connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1){
    return false;
  }
  return true;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd= -1;
}


/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
  //cd cmpsc311/assignment5-CaixuanWang
  if((send_packet(cli_sd,op,block)==false)){
      return -1;
  }
  uint16_t ret;
  if((recv_packet(cli_sd,&op,&ret,block))==false){
    return -1;
  }
  return 0;
}