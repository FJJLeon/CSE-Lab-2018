// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#define debug 1

lock_server::lock_server():
  nacquire (0)
{
  //mutex = PTHREAD_MUTEX_INITIALIZER;
  //cond = PTHREAD_COND_INITIALIZER;
  pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  r = 0;

  if (debug)
    printf("lock_server::acquire clt %d, lockid %llu\n", clt, lid);

  pthread_mutex_lock(&mutex);

  std::map<lock_protocol::lockid_t, bool>::iterator iter;
  iter = lock_table.find(lid);
  if (iter != lock_table.end()) {
    // acquire need wait
    while (lock_table[lid] == true){
      pthread_cond_wait(&cond, &mutex);
    }
  }
  //iter->second = true;
  lock_table[lid] = true;
  nacquire++;

  pthread_mutex_unlock(&mutex);

  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  r = 0;

  if (debug)
    printf("lock_server::release clt %d, lockid %llu\n", clt, lid);

  pthread_mutex_lock(&mutex);
  
  std::map<lock_protocol::lockid_t, bool>::iterator iter;
  iter = lock_table.find(lid);
  if (iter != lock_table.end() && iter->second == true) {
    // lid exist and is locked, release it
    //iter->second = false;
    lock_table[lid] = false;
    pthread_cond_broadcast(&cond);
  }
  else {
    ret = lock_protocol::NOENT;
    if (debug)
      printf("error lock_server::release want release a not locked lid %llu", lid);
  }

  pthread_mutex_unlock(&mutex);
  return ret;
}
