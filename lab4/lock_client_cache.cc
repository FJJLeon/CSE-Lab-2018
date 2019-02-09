// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include <unistd.h>

#define debug 0

#define debugout(args...) do { \
        if (debug)  \
          tprintf(args);   \
        } while (0);

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  char hname[100];
  VERIFY(gethostname(hname, sizeof(hname)) == 0);
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  // register revoke and retry rpc 
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  pthread_mutex_init(&mutex, NULL);
	//pthread_cond_init(&cond, NULL);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  // ret = cl->call(lock_protocol::acquire, cl->id(), lid, ret);
  debugout("lcc::acquire clt %s acquire lid %llu\n", id.c_str(), lid);

  pthread_mutex_lock(&mutex);

  std::map<lock_protocol::lockid_t, client_state>::iterator iter;
  iter = lock_table.find(lid);
  if (iter == lock_table.end()) {
    client_state newcs = {client_state::NONE};
    pthread_cond_init(&newcs.innerbusy, NULL);
    pthread_cond_init(&newcs.outerbusy, NULL);
    lock_table[lid] = newcs;
  }
  
  switch(lock_table[lid].state) {
    case client_state::NONE: case client_state::REVOKING:
    { // add REVOKING in seems incorrect!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      debugout("clt %s lock %llu is NONE REVOKING\n", id.c_str(), lid);

      lock_table[lid].state = client_state::ACQUIRING;
      pthread_mutex_unlock(&mutex);
      ret = cl->call(lock_protocol::acquire, lid, id, ret);
      //pthread_mutex_lock(&mutex);
      // what if ret is RETRY
      VERIFY(ret == lock_protocol::OK || ret == lock_protocol::RETRY);
      if (ret == lock_protocol::RETRY) {
        //debugout("other clt own lid %llu, clt %s sleep\n", lid, id.c_str());
        //pthread_cond_wait(&lock_table[lid].outerbusy, &mutex);
        // when wake, lock must be yours, right?
        while (ret == lock_protocol::RETRY) {
          //pthread_mutex_unlock(&mutex);
          sleep(1);
          debugout("lcc::acquire clt %s acquire lid %llu again\n", id.c_str(), lid);
          ret = cl->call(lock_protocol::acquire, lid, id, ret);
          //pthread_mutex_lock(&mutex);
          if (ret == lock_protocol::OK)
            break;
        }
      }
      pthread_mutex_lock(&mutex);
      VERIFY(lock_table[lid].state != client_state::NONE);
      lock_table[lid].state = client_state::LOCKED;
      break;
    }
    case client_state::FREE:
    {
      debugout("clt %s lock %llu is FREE\n", id.c_str(), lid);
      lock_table[lid].state = client_state::LOCKED;
      break;
    }
    case client_state::LOCKED: case client_state::ACQUIRING:
    {// lock is owned by the client, but not this thread
      debugout("clt %s lock %llu is LOCKED ACQUIRING\n", id.c_str(), lid);
      //if (lock_table[lid].state == client_state::LOCKED)
      // wait other thread free lock
      while (lock_table[lid].state == client_state::LOCKED
          || lock_table[lid].state == client_state::ACQUIRING) {
        pthread_cond_wait(&lock_table[lid].innerbusy, &mutex);
      }
      if (lock_table[lid].state == client_state::FREE) {
        lock_table[lid].state = client_state::LOCKED;
      }
      else {
        tprintf("sth terrible happen, in %s lock %llu release but a thread want\n"
                , id.c_str(), lid);
      }
      break;
    }
    default:
      assert(0);
  }
  
  VERIFY(ret == lock_protocol::OK || ret == lock_protocol::RETRY);
  //debugout("hhhhh\n");
  pthread_mutex_unlock(&mutex);
  return ret;
}

/*
 * lock_client_cache::release is called when a thread free a lock
 * but different from origin, the lock is still held by the client
 * so in this function no need rpc to server(correct??????????)
 * besides, there may exists other threads waiting for the lock as well, signal them
 */
lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  
  pthread_mutex_lock(&mutex);
  debugout("lcc::release clt %s release %llu\n", id.c_str(), lid);
  if (lock_table[lid].state != client_state::LOCKED)
    printf("ERROR: release a unheld lock %llu\n", lid);
  lock_table[lid].state = client_state::FREE;
  // signal only wake one thread?
  pthread_cond_signal(&lock_table[lid].innerbusy);
  pthread_mutex_unlock(&mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int ret = rlock_protocol::OK;
  int r;
  debugout("lcc::revoke_handler clt %s revoke lid %llu not mutex\n", id.c_str(), lid);
  while (lock_table[lid].state != client_state::LOCKED && lock_table[lid].state != client_state::FREE);
  pthread_mutex_lock(&mutex);
  //debugout("lcc::revoke_handler clt %s revoke lid %llu\n", id.c_str(), lid);
  // the lock may still locked by other thread, wait
  while (lock_table[lid].state == client_state::LOCKED) {
    debugout("lcc::revoke_handler lid %llu LOCKED, wait\n", lid);
    pthread_cond_wait(&lock_table[lid].innerbusy, &mutex);
  }
    
  debugout("lcc::revoke clt %s begin rpc server or wake up\n", id.c_str());
  lock_table[lid].state = client_state::REVOKING;

  pthread_mutex_unlock(&mutex);
  lock_protocol::status tmpret = cl->call(lock_protocol::release, lid, id, r);
  VERIFY(tmpret == lock_protocol::OK);
  pthread_mutex_lock(&mutex);

  lock_table[lid].state = client_state::NONE;
  pthread_mutex_unlock(&mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int ret = rlock_protocol::OK;
  int r;
  pthread_mutex_lock(&mutex);
  debugout("lcc::retry_handler clt %s retry lid %llu\n", id.c_str(), lid);

  pthread_mutex_unlock(&mutex);
  ret = cl->call(lock_protocol::acquire, lid, id, r);
  pthread_mutex_lock(&mutex);
  VERIFY(ret == lock_protocol::OK);
  //pthread_cond_signal(&lock_table[lid].outerbusy);
  
  pthread_mutex_unlock(&mutex);
  lock_table[lid].state = client_state::LOCKED;
  return ret;
}

