#include "datanode.h"
#include <arpa/inet.h>
#include "extent_client.h"
#include <unistd.h>
#include <algorithm>
#include "threader.h"

using namespace std;

#define debug 1

#define debugout(args...) do { \
        if (debug)  \
          fprintf(stderr, args);   \
        } while (0);


int DataNode::init(const string &extent_dst, const string &namenode, const struct sockaddr_in *bindaddr) {
  ec = new extent_client(extent_dst);

  // Generate ID based on listen address
  id.set_ipaddr(inet_ntoa(bindaddr->sin_addr));
  id.set_hostname(GetHostname());
  id.set_datanodeuuid(GenerateUUID());
  id.set_xferport(ntohs(bindaddr->sin_port));
  id.set_infoport(0);
  id.set_ipcport(0);

  // Save namenode address and connect
  make_sockaddr(namenode.c_str(), &namenode_addr);
  if (!ConnectToNN()) {
    delete ec;
    ec = NULL;
    return -1;
  }

  // Register on namenode
  if (!RegisterOnNamenode()) {
    delete ec;
    ec = NULL;
    close(namenode_conn);
    namenode_conn = -1;
    return -1;
  }

  /* Add your initialization here */

  // create a thread to send heartbeat periodically
  //SendHeartbeat();
  NewThread(this, &DataNode::periodSendHeartbeat);
  //NewThread(this, &DataNode::SendHeartbeat)
  return 0;
}

bool DataNode::ReadBlock(blockid_t bid, uint64_t offset, uint64_t len, string &buf) {
  /* Your lab4 part 2 code */
  debugout("DataNode::ReadBlock\n");

  string tmp;
  if (ec->read_block(bid, tmp) != extent_protocol::OK) {
    debugout("ERROR DataNode::ReadBlock read_block failed\n");
    return false;
  }

  buf = tmp.substr(offset, len);

  return true;
}

bool DataNode::WriteBlock(blockid_t bid, uint64_t offset, uint64_t len, const string &buf) {
  /* Your lab4 part 2 code */
  debugout("DataNode::WriteBlock\n");

  string tmp;
  if (ec->read_block(bid, tmp) != extent_protocol::OK) {
    debugout("ERROR DataNode::WriteBlock read_block failed\n");
    return false;
  }
  debugout("WriteBlock: before %s\n", tmp.c_str());
  tmp = tmp.replace(offset, len, buf);
  debugout("WriteBlock: after %s\n", tmp.c_str());
  if (ec->write_block(bid, tmp) != extent_protocol::OK) {
    debugout("ERROR DataNode::WriteBlock read_block failed\n");
    return false;
  }

  return true;
}

