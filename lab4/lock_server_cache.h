#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <map>
#include <set>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

struct server_state 
{
  enum {FREE, GRANTED, REVOKING}state; // state each lock
  std::string id;                      // who hold it (if any): hostname::port
  std::set<std::string> wait_queue;    // who wait it (if any)
  std::string next;                    // scheduled next client to hold this lock
};

class lock_server_cache {
 private:
  int nacquire;
  std::map<lock_protocol::lockid_t, server_state> lock_table;
  // rpcc *rlscl; not right, server need connect with a lot client, see handle.h
  pthread_mutex_t mutex;
  pthread_cond_t cond;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
