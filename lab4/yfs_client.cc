// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define debug 1

#define debugout(args...) do { \
        if (debug)  \
          fprintf(stderr, args);   \
        } while (0);


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  //lc = new lock_client(lock_dst);
  lc = new lock_client_cache(lock_dst);
  if (debug)
    printf("what yfs_client ex %s, lo %s\n", extent_dst.c_str(), lock_dst.c_str());
  
  VERIFY(acquire(1) == lock_protocol::OK);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
  VERIFY(release(1) == lock_protocol::OK);
}

int
yfs_client::acquire(inum inum)
{
    return lc->acquire(inum);
}

int
yfs_client::release(inum inum)
{
    return lc->release(inum);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;
    bool flag = false;

//VERIFY(acquire(inum) == lock_protocol::OK);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        flag = true;
    } 
    else {
        printf("isfile: %lld is not a file\n", inum);
    }
    

//VERIFY(release(inum) == lock_protocol::OK);
    return flag;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?????????????????? no
    extent_protocol::attr a;
    bool flag = false; 

//VERIFY(acquire(inum) == lock_protocol::OK);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isdir: %lld is a dir\n", inum);
        flag = true;
    } 
    else {
        printf("isdir: %lld is not a dir\n", inum);
    }
    
//VERIFY(release(inum) == lock_protocol::OK);
    return flag;
}

bool
yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;
    bool flag = false; 

//VERIFY(acquire(inum) == lock_protocol::OK);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
    }

    if (a.type == extent_protocol::T_SYMLINK) {
        printf("issymlink: %lld is a symlink\n", inum);
        flag = true;
    } 
    else {
        printf("issymlink: %lld is not a symlink\n", inum);
    }
    
//VERIFY(release(inum) == lock_protocol::OK);
    return flag;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;
//VERIFY(acquire(inum) == lock_protocol::OK);
    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
//VERIFY(release(inum) == lock_protocol::OK);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;
//VERIFY(acquire(inum) == lock_protocol::OK);
    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
//VERIFY(release(inum) == lock_protocol::OK);
    return r;
}

int 
yfs_client::getsymlink(inum inum, syminfo& sin) 
{
    int r = OK;

    printf("getsymlink %016llx\n", inum);
    extent_protocol::attr a;
//VERIFY(acquire(inum) == lock_protocol::OK);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    sin.atime = a.atime;
    sin.mtime = a.mtime;
    sin.ctime = a.ctime;
    sin.size  = a.size;
    printf("getsymlink %016llx -> sz %llu\n", inum, sin.size);

release:
//VERIFY(release(inum) == lock_protocol::OK);
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;
    if (debug)
        printf("debug: yfs_client::setattr ino %llu, new size %lu\n", ino, size);
    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    // param check
    if (!isfile(ino)) {
        printf("error: yfs_client::setattr ino not a file\n");
        return IOERR;
    }

VERIFY(acquire(ino) == lock_protocol::OK);
    std::string buf;
    r = ec->get(ino, buf);
    if (r != OK) {
        printf("error: yfs_client::setattr get data wrong\n");
        goto release;
    }
    // change size, resize will check, and add \0 if less
    buf.resize(size);

    r = ec->put(ino, buf);
    if (r != OK) {
        printf("error: yfs_client::setattr write back wrong\n");
        goto release;
    }
release:
VERIFY(release(ino) == lock_protocol::OK);
    return r;
}

int
yfs_client::createORmkdir(inum parent, const char *name, mode_t mode, inum &ino_out, uint32_t type)
{
    debugout("\tyfs_client::createORmkdir parent:%llu, name:%s, type:%d\n", parent, name, type);
    int r = OK;
    //param check
    if (!isdir(parent)) 
        return IOERR;
    if (!name)
        return IOERR;
    // check already exist
    bool found = false;
    std::string entry, buf;

VERIFY(acquire(parent) == lock_protocol::OK);
    r = lookup(parent, name, found, ino_out);

    if (r != OK) {
        printf("error: yfs_client::createORmkdir lookup\n");
        goto release;
    }

    if (found) {
        printf("error: yfs_client::createORmkdir file or dir already exists\n");
        goto release;
    }
//VERIFY(acquire(0) == lock_protocol::OK);
    ec->create(type, ino_out);

    debugout("\tyfs_client::createORmkdir what given ino_out:%d\n", ino_out);

    r = ec->get(parent, buf);
    if (r != OK) {
        printf("error: yfs_client::createORmkdir get parent wrong\n");
        goto release;
    }
//VERIFY(release(0) == lock_protocol::OK);
    entry = std::string(name) + '|' +  filename(ino_out) + '|';
    buf.append(entry);
    r = ec->put(parent, buf);
    if (r != OK) {
        printf("error: yfs_client::createORmkdir put to parent failed\n");
        goto release;
    }
    if (debug)
        printf("!!debug: yfs_client::createORmkdir give name %s inode num %d\n", name, (int)ino_out);
release:
VERIFY(release(parent) == lock_protocol::OK);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    printf("!!yfs_client::create name %s in dir inum %d\n", name, (int)parent);
    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    r = createORmkdir(parent, name, mode, ino_out, extent_protocol::T_FILE);

    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    printf("!!yfs_client::mkdir name %s in inum %d\n", name, (int)parent);
    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    r = createORmkdir(parent, name, mode, ino_out, extent_protocol::T_DIR);

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    printf("!!yfs_client::lookup in %016llx for %s\n", parent, name);
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    found = false;

    if (!isdir(parent)) 
        return IOERR;
    if (!name)
        return r;

    std::list<dirent> entrylist;
    r = readdir(parent, entrylist);
    if (r != OK) {
        printf("error: yfs_client::lookup readdir\n");
        return r;
    }
    std::string aim_name = std::string(name);
    std::list<dirent>::iterator it;
    for (it = entrylist.begin(); it != entrylist.end(); it++) {
        if (it->name == aim_name) {
            found = true;
            ino_out = it->inum;
            break;
        }
    }

    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;
    printf("readdir %016llx\n", dir);
    list.clear();
    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    
    if (!isdir(dir))
        return IOERR;
//VERIFY(acquire(dir) == lock_protocol::OK);
    struct dirent *entry = new dirent();
    size_t head = 0;
    size_t tail = 0;

    std::string buf;
    r = ec->get(dir, buf);

    if (r != OK) {
        printf("error: yfs_client::readdir get\n");
        goto release;
    }

    while (head < buf.size()) {
        tail = buf.find('|', head);
        std::string name = buf.substr(head, tail - head);
        entry->name = name;
        head = tail + 1;

        tail = buf.find('|', head);
        std::string inum = buf.substr(head, tail - head);
        entry->inum = n2i(inum);
        head = tail + 1;

        list.push_back(*entry);
    }
    delete entry;
release:
//VERIFY(release(dir) == lock_protocol::OK);
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;
    if (debug)
        printf("debug: yfs_client::read inum %d from %d with size %d\n", (int)ino, (int)off, (int)size);
    /*
     * your code goes here.
     * note: read using ec->get().
     */
    //param check
    if (!isfile(ino)) 
        return IOERR;

VERIFY(acquire(ino) == lock_protocol::OK);
    // get file content
    std::string buf;
    r = ec->get(ino, buf);
    if (r != OK) {
        printf("error: yfs_client::read ec->get wrong\n");
        goto release;
    }
    // substr will check param whaterver off > len
    data = buf.substr(off, size);
release:
VERIFY(release(ino) == lock_protocol::OK);
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
    if (debug)
        printf("debug yfs_client::write inum %d from %d with size %d\n", (int)ino, (int)off, (int)size);
    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    //param check
    if (!isfile(ino)) {
        printf("error: yfs_client::write ino is not a file\n");
        return IOERR;
    }
    if (size < 0 || off < 0){
        printf("error: yfs_client::write size or off < 0\n");
        return IOERR;
    }
VERIFY(acquire(ino) == lock_protocol::OK);
//VERIFY(acquire(0) == lock_protocol::OK);
    // get file content
    std::string buf;
    size_t len;
    r = ec->get(ino, buf);
    if (r != OK) {
        printf("error: yfs_client::write get origin data wrong\n");
        goto release;
    }
    // change content
    len = buf.size();
    /*
    if (off + size <= len)
        buf.replace(off, size, std::string(data, size));
    else {
        buf.resize(off + size);
        buf.replace(off, size, std::string(data, size));
    }
    */
    // when newlen > oldlen, expand the content and replace with data 
    if (off + size > len)
        buf.resize(off + size);
    buf.replace(off, size, std::string(data, size)); //data has \0 ? need size


    r = ec->put(ino, buf);
    if (r != OK) {
        printf("error: yfs_client::write write back wrong\n");
        goto release;
    }
    bytes_written = size;

release:
//VERIFY(release(0) == lock_protocol::OK);
VERIFY(release(ino) == lock_protocol::OK);
    return r;
}

int 
yfs_client::unlink(inum parent,const char *name)
{

    int r = OK;
    if (debug)
        printf("debug: yfs_client::unlink file %s from parent %llu\n", name, parent);
    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    
    if (!isdir(parent)) {
        printf("error: yfs_client::unlink parent %llu not dir\n", parent);
        return IOERR;
    }
//VERIFY(acquire(parent) == lock_protocol::OK);
//VERIFY(acquire(0) == lock_protocol::OK);
    bool found = false;
    inum ino_out;
    r = lookup(parent, name, found, ino_out);
    if (r != OK) {
        printf("error: yfs_client::unlink lookup failed\n");
        return r;
    }
    if (!found) {
        printf("error: yfs_client::unlink file %s not exists in parent %llu \n", name, parent);
        r = NOENT;
        return r;
    }
//VERIFY(acquire(ino_out) == lock_protocol::OK);
    if (isdir(ino_out)) {
        printf("error: yfs_client::unlink file: %s, inum %lld, is a dir, can not unlink dir\n", name, ino_out);
        return IOERR;
    }
    if (debug)
        printf("debug: yfs_client::unlink file %s, inum %llu\n", name, ino_out);

    std::string aim_name = std::string(name);
    std::string parent_buf;
    size_t pos, len;
    r = ec->get(parent, parent_buf);
    if (r != OK) {
        printf("error: yfs_client::unlink get parent wrong\n");
        goto release;
    }

    r = ec->remove(ino_out);

    if (r != OK) {
        printf("error: yfs_client::unlink remove failed\n");
        goto release;
    }
//VERIFY(release(ino_out) == lock_protocol::OK);
    pos = parent_buf.find(aim_name);
    if (pos == std::string::npos){
        printf("error: yfs_client::unlink find failed\n");
        r = IOERR;
        goto release;
    }
    len = aim_name.size() + filename(ino_out).size() + 2;
    parent_buf.erase(pos, len);

    r = ec->put(parent, parent_buf);
    if (r != OK) {
        printf("error: yfs_client::unlink write back to parent %llu failed\n", parent);
        goto release;
    }
release:
//VERIFY(release(0) == lock_protocol::OK);
//dddVERIFY(release(parent) == lock_protocol::OK);
    return r;
}

int 
yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out)
{
    int r = OK;
    if (debug)
        printf("debug: yfs_client::symlink link %s to %s in parent %llu\n", link, name, parent);

    r = createORmkdir(parent, name, 0, ino_out, extent_protocol::T_SYMLINK);
    if (r != OK) {
        printf("error: yfs_client::symlink create failed\n");
        return r;
    }
VERIFY(acquire(parent) == lock_protocol::OK);
    r = ec->put(ino_out, std::string(link));
    if (r != OK) {
        printf("error: yfs_client::symlink put failed\n");
        goto release;
    }
release:
VERIFY(release(parent) == lock_protocol::OK);
    return r;
}

int 
yfs_client::readlink(inum ino, std::string &link)
{
    int r = OK;
    

    if (!issymlink(ino)) {
        link.clear();
        return IOERR;
    }
VERIFY(acquire(ino) == lock_protocol::OK);
    r = ec->get(ino, link);
    if (r != OK) {
        printf("error: yfs_client::readlink get failed\n");
        goto release;
    }
release:
VERIFY(release(ino) == lock_protocol::OK);
    return r;
}