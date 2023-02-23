#include "student.h"

int largest(int array[], int length) {
  int i=0;
  int result;
  while(i<length){
    if(i==0){
      result=array[i];
    }
    if(result<array[i]){
      result=array[i];
    }
    i++;
  }
  return(result);
}

int sum(int array[], int length) {
  int i=0;
  int result=0;
  if(length==0){
    return(0);
  }
  while(i<length){
    result=result+array[i];
    i++;
  }
  return(result);
}

void swap(int *a, int *b) {
  int i;
  i=*a;
  *a=*b;
  *b=i;
}

void rotate(int *a, int *b, int *c) {
  int i;
  i=*a;
  *a=*c;
  *c=*b;
  *b=i;
}

void sort(int array[], int length) {
  int i, j;
  for(i=0;i<length-1;i++){
    for(j=0;j<length-i-1;j++){
      if(array[j]>array[j+1]){
        swap(&array[j],&array[j+1]);
      }
    }
  }
}

int prime_check(int number){
  int i;
  if(number<2){
    return(1);
  }
  for(i=2;i<=number/2;++i){
    if(number%i==0){
      return(1);
    }
  }
  return(0);
}
void double_primes(int array[], int length) {
  int i=0;
  while(i<length){
    if(prime_check(array[i])==0){
      array[i]=2*array[i];
    }
    i++;
  }
}
int power(int x, int y){
  int result=1,i=0;
  while(i<y){
    result=result*x;
    i++;
  }
  return(result);
}
int armstrong_check(int number){
  int i,remainder,degit=0,result=0;
  if(number<=0){
    return(1);
  }
  for(i=number;i!=0;++degit){
    i/=10;
  }
  for(i=number;i!=0;i/=10){
    remainder=i%10;
    result+=power(remainder,degit);
  }
  if(result==number){
    return(0);
  }
  else{
    return(1);
  }
}
void negate_armstrongs(int array[], int length) {
  int i=0;
  while(i<length){
    if(armstrong_check(array[i])==0){
      array[i]-=array[i]*2;
    }
    i++;
  }
}
