// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

#define debug 0

#define debugout(args...) do { \
        if (debug)  \
          tprintf(args);   \
        } while (0);


lock_server_cache::lock_server_cache():
  nacquire(0)
{
  pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &r)
{ // client (id) acquire lock(lid)
	lock_protocol::status ret = lock_protocol::OK;
  r = 0;

  debugout("lsc::acquire, client %s acquire lid %llu\n", id.c_str(), lid);

  pthread_mutex_lock(&mutex);

  std::map<lock_protocol::lockid_t, server_state>::iterator iter;
  iter = lock_table.find(lid);
  if (iter == lock_table.end()) {
    // lock not exist, first time to acquire this lock
    debugout("lock not exist\n");
    std::set<std::string> wait_queue;
    server_state newls = {server_state::GRANTED, id, wait_queue};
    
    lock_table[lid] = newls;
    debugout("now state %d\n", lock_table[lid].state);
  }
  else {
    debugout("client %s lock %llu is %d\n", id.c_str(), lid, lock_table[lid].state);
    switch (lock_table[lid].state) {
      case server_state::FREE:
      {
        debugout("client %s lock %llu is FREE\n", id.c_str(), lid);
        // note!! free but not yours
        // VERIFY(id == lock_table[lid].next);
        if (id != lock_table[lid].next) {
          pthread_mutex_unlock(&mutex);
          return lock_protocol::RETRY;
        }
        VERIFY(id == lock_table[lid].next);
        lock_table[lid].next.clear();
        VERIFY(lock_table[lid].next.empty());
        if (!lock_table[lid].wait_queue.empty()) {
          // reset next if any
          lock_table[lid].next = *lock_table[lid].wait_queue.begin();
          lock_table[lid].wait_queue.erase(lock_table[lid].wait_queue.begin());
        }
        debugout("clt %s get FREE lock %llu\n", id.c_str(), lid);
        lock_table[lid].state = server_state::GRANTED;
        lock_table[lid].id = id;
        pthread_mutex_unlock(&mutex);
        return lock_protocol::OK;
      }
      // GRANTED mens no one waiting for the lock while one hold it
      case server_state::GRANTED:
      { // lock is owned by other client, send revoke rpc
        debugout("client %s lock %llu is GRANTED\n", id.c_str(), lid);
        VERIFY(id != lock_table[lid].id);
        lock_table[lid].state = server_state::REVOKING;
        //lock_table[lid].wait_queue.insert(id);
        lock_table[lid].next = id;
        // see handle.h for how to connect with a lot of clients
        pthread_mutex_unlock(&mutex);

        handle h(lock_table[lid].id);
        rpcc *cl = h.safebind();
        rlock_protocol::status tmpret;
        if(cl){
          debugout("revoke call to client %s lock %llu\n", lock_table[lid].id.c_str(), lid);
          tmpret = cl->call(rlock_protocol::revoke, lid, r);
          //pthread_mutex_lock(&mutex);
        } else {
          printf("ERROR: lsc revoke rpc to lcc failed\n");
        }
        VERIFY(tmpret == rlock_protocol::OK);
        //pthread_mutex_unlock(&mutex);
        debugout("lsc::acquire, client %s acquire lid %llu return OK\n", id.c_str(), lid);
        return lock_protocol::OK;
      }
      case server_state::REVOKING:
      { // lock is owned by other clientm, and revoke rpc already sent
        debugout("client %s lock %llu is REVOKING\n", id.c_str(), lid);
        if (lock_table[lid].wait_queue.find(id) != lock_table[lid].wait_queue.end()) {
          debugout("why this client %s acquire again\n", id.c_str());
        }
        else if (id == lock_table[lid].next) {
          debugout("clt %s are next to lock %llu, why again\n", id.c_str(), lid);
        }
        else
          lock_table[lid].wait_queue.insert(id);
        printf("after %s, %lu client waiting\n", id.c_str(), lock_table[lid].wait_queue.size());
        ret = lock_protocol::RETRY;
        pthread_mutex_unlock(&mutex);
        return lock_protocol::RETRY;
      }
      default:
        assert(0);
    }
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

/*
 * lock_server_cache::release is called when some client want a granted lock (after revoke)
 * so a retry RPC should be sent be scheduled next client  
 */
int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  debugout("lsc::release clt %s revoke lid %llu\n", id.c_str(), lid);
  pthread_mutex_lock(&mutex);
  VERIFY(lock_table[lid].id == id);
  VERIFY(lock_table[lid].state == server_state::REVOKING);

  lock_table[lid].state = server_state::FREE;
  //lock_table[lid].id = nullptr;
  std::string next = lock_table[lid].next;
  pthread_mutex_unlock(&mutex);

  handle h(next);
  rpcc *cl = h.safebind();
  rlock_protocol::status tmpret;
  if(cl){
    tmpret = cl->call(rlock_protocol::retry, lid, r);
  } else {
    printf("ERROR: lsc revoke rpc to lcc failed\n");
  }
  VERIFY(tmpret == rlock_protocol::OK);
  debugout("lsc::release clt %s revoke lid %llu return\n", id.c_str(), lid);
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

