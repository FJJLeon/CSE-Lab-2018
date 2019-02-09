#include "namenode.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sys/stat.h>
#include <unistd.h>
#include "threader.h"
#include <algorithm>

using namespace std;

#define debug 1

#define debugout(args...) do { \
        if (debug)  \
          fprintf(stderr, args);   \
        } while (0);

void NameNode::init(const string &extent_dst, const string &lock_dst) {
  ec = new extent_client(extent_dst);
  lc = new lock_client_cache(lock_dst);
  yfs = new yfs_client(extent_dst, lock_dst);

  debugout("HHHHHHHHHHH\n");
  /* Add your init logic here */
  // initial master heartbeadt to 0
  master_hb = 0;

  //registerDNs.push_back(master_datanode);
  //liveDNs.push_back(master_datanode);
  
}

list<NameNode::LocatedBlock> NameNode::GetBlockLocations(yfs_client::inum ino) {
  debugout("NameNode::LocatedBlock\n");

  std::list<blockid_t> block_ids;
  if (ec->get_block_ids(ino, block_ids) != extent_protocol::OK) {
    debugout("ERROR, NameNode::LocatedBlock get_block_ids failed\n");
    return list<LocatedBlock>();
  }

  extent_protocol::attr a;
  if (ec->getattr(ino, a) != extent_protocol::OK) {
    debugout("ERROR, NameNode::LocatedBlock getattr failed\n");
    return list<LocatedBlock>();
  }

  uint64_t size = a.size;
  uint64_t offset = 0;
  list<LocatedBlock> LBlist;
  std::list<blockid_t>::iterator iter;
  for (iter=block_ids.begin(); iter != block_ids.end(); iter++) {
    if (size >= BLOCK_SIZE) {
      LBlist.push_back(LocatedBlock(*iter, offset, BLOCK_SIZE, liveDNs));
    }
    else {
      LBlist.push_back(LocatedBlock(*iter, offset, size, liveDNs));
    }
    size -= BLOCK_SIZE;
    offset += BLOCK_SIZE;
  }

  return LBlist;
}

bool NameNode::Complete(yfs_client::inum ino, uint32_t new_size) {
  debugout("NameNode::Complete,  ino:%llu, new_size:%d\n", ino, new_size);

  if (ec->complete(ino, new_size) != extent_protocol::OK) {
    debugout("ERROR: NameNode::Complete failed\n");
    return false;
  }
  
  if (lc->release(ino) != lock_protocol::OK) {
    debugout("ERROR: NameNode::Complete release failed\n");
    return false;
  }

  appendOffset[ino] = -1;

  return true;
}

NameNode::LocatedBlock NameNode::AppendBlock(yfs_client::inum ino) {
  debugout("NameNode::AppendBlock\n");

  blockid_t bid;
  if (ec->append_block(ino, bid) != extent_protocol::OK) {
    debugout("ERROR: NameNode::AppendBlock append_block failed\n");
    throw HdfsException("append_block failed");
  }

  
  // record written blocks for replicating 
  replicated_blocks.push_back(bid);

  //uint64_t size = a.size;
  // convert to LocatedBlock
  LocatedBlock lbk = LocatedBlock(bid, appendOffset[ino], BLOCK_SIZE, liveDNs);
  appendOffset[ino] += BLOCK_SIZE;

  return lbk; 
  //throw HdfsException("Not implemented");
}

std::string
filename(yfs_client::inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool NameNode::Rename(yfs_client::inum src_dir_ino, string src_name, yfs_client::inum dst_dir_ino, string dst_name) {
  debugout("\tNameNode::Rename, src_dir: %llu, src_name:%s, dst_dir_ino:%llu, dst_name:%s\n",
            src_dir_ino, src_name.c_str(), dst_dir_ino, dst_name.c_str());

  // check already exist
  bool found = false;
  std::string entry, buf;
  yfs_client::inum exist_ino;
  int r;

  yfs->lookup(src_dir_ino, src_name.c_str(), found, exist_ino);
  debugout("\tNN:Rename, exist ino: %llu\n", exist_ino);
  if (!found) {
    debugout("ERROR:NameNode::Rename, not found in src_dir_ino\n");
    return false;
  }
  // cannot unlink straight
  // yfs->unlink(src_dir_ino, src_name.c_str());
  
  // erase from src
  r = ec->get(src_dir_ino, buf);
  if (r != extent_protocol::OK) {
        printf("ERROR:NameNode::Rename get parent wrong\n");
        return false;
    }
  
  size_t pos = buf.find(src_name);
  size_t len = src_name.size() + filename(exist_ino).size() + 2;
  buf.erase(pos, len);

  r = ec->put(src_dir_ino, buf);
  if (r != extent_protocol::OK) {
        printf("ERROR:NameNode::Rename remove failed\n");
        return false;
    }
  // add to dst
  yfs_client::inum dummy_ino;
  r = yfs->lookup(dst_dir_ino, dst_name.c_str(), found, dummy_ino);
  if (found) {
    debugout("ERROR:NameNode::Rename, already exist in dst_dir\n");
    return false;
  }

  r = ec->get(dst_dir_ino, buf);
  debugout("\tNN:Rename, all entry:%s\n", buf.c_str());
  if (r != extent_protocol::OK) {
    printf("ERROR:NameNode::Rename get parent wrong\n");
    return false;
  }
  entry = std::string(dst_name) + '|' +  filename(exist_ino) + '|';
  debugout("\tNN:Renmae, new entry:%s\n", entry.c_str());
  buf.append(entry);
  r = ec->put(dst_dir_ino, buf);
  if (r != extent_protocol::OK) {
      printf("ERROR:NameNode::Rename put to parent failed\n");
      return false;
  }

  return true;
}

bool NameNode::Mkdir(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
  debugout("NameNode::Mkdir in parent:%llu, name: %s \n", parent, name.c_str());

  yfs->mkdir(parent, name.c_str(), mode, ino_out);

  debugout("\tNameNode::Mkdir get ino_out: %llu\n", ino_out);
  return true;
}

bool NameNode::Create(yfs_client::inum parent, string name, mode_t mode, yfs_client::inum &ino_out) {
  debugout("NameNode::Create in parent:%llu, name: %s \n", parent, name.c_str());

  yfs->create(parent, name.c_str(), mode, ino_out);
  lc->acquire(ino_out);

  extent_protocol::attr a;
  if (ec->getattr(ino_out, a) != extent_protocol::OK) {
    debugout("ERROR, NameNode::AppendBlock getattr failed\n");
    throw HdfsException("getattr failed");
  }

  appendOffset[ino_out] = a.size;

  debugout("\tNameNode::Create get ino_out: %llu, size: %u\n", ino_out, a.size);
  return true;
}

bool NameNode::Isfile(yfs_client::inum ino) {
  debugout("NameNode::Isfile ino:%llu\n",ino);

  return yfs->isfile(ino);

  //return true;
}

bool NameNode::Isdir(yfs_client::inum ino) {
  debugout("NameNode::Isdir ino:%llu\n",ino);

  return yfs->isdir(ino);

  //return true;
}

bool NameNode::Getfile(yfs_client::inum ino, yfs_client::fileinfo &info) {
  debugout("NameNode::Getfile ino:%llu\n",ino);

  yfs->getfile(ino, info);

  return true;
}

bool NameNode::Getdir(yfs_client::inum ino, yfs_client::dirinfo &info) {
  debugout("NameNode::Getdir ino:%llu\n",ino);

  yfs->getdir(ino, info);

  return true;
}

bool NameNode::Readdir(yfs_client::inum ino, std::list<yfs_client::dirent> &dir) {
  debugout("NameNode::Readdir ino:%llu\n",ino);

  yfs->readdir(ino, dir);

  return true;
}

bool NameNode::Unlink(yfs_client::inum parent, string name, yfs_client::inum ino) {
  debugout("NameNode::Unlink in parent:%llu, name:%s, ino:%llu\n",parent, name.c_str(), ino);

  yfs->unlink(parent, name.c_str());

  return true;
}

void NameNode::DatanodeHeartbeat(DatanodeIDProto id) {
  debugout("\tNameNode::DatanodeHeartbeat receive heartbeat\n");
  std::map<DatanodeIDProto, uint32_t>::iterator iter;
  
  iter = registerDNs.find(id);
  if (iter == registerDNs.end()) {
    fprintf(stderr, "\tERROR NameNode::DatanodeHeartbeat unknown heartbeat\n");
    return;
  }
  registerDNs[id] += 1;
  // in master datanode, check master_hb with other datanode heartbeat
  if (id == master_datanode) {
    master_hb = registerDNs[id];
    for (iter=registerDNs.begin(); iter!=registerDNs.end(); iter++) {
      // dead datanode detect
      if (master_hb - iter->second >= 4) {
        debugout("\tNameNode::DatanodeHeartbeat detect someone died\n");
        if (find(liveDNs.begin(), liveDNs.end(), iter->first) != liveDNs.end())
          liveDNs.remove(iter->first);
      }
    }
  }

  int index = 0;
  for (iter=registerDNs.begin(); iter!=registerDNs.end(); iter++) {
    if (iter->first == id) {
      debugout("\tWho sent heartbeat : %d\n", index);
    }
    
    if (iter->first == master_datanode) {
      debugout("\tI'm master node: %d\n", index);
    }
    index++;
  } 

}

void NameNode::RegisterDatanode(DatanodeIDProto id) {
  debugout("NameNode::RegisterDatanode\n");

  // new datanode heartbeat set to master_hb
  // note: cannot use insert
  // replicate written blocks into new datanode
  if (!replicated_blocks.empty()) {
    int count = replicated_blocks.size();
    for (int i=0; i<count; i++) {
      if (ReplicateBlock(replicated_blocks[i], master_datanode, id) != true){
        fprintf(stderr, "\tNameNode::RegisterDatanode, replicate failed");
        return ;
      }
    }
  }
  registerDNs[id] = master_hb;
  liveDNs.push_back(id);
  return;
}

list<DatanodeIDProto> NameNode::GetDatanodes() {

  return liveDNs;
  //return list<DatanodeIDProto>();
}
