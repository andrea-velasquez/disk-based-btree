#pragma once

#include "pagemanager.h"
#include <memory>

namespace utec {
namespace disk {

template <class T, int BTREE_ORDER = 3> class btree {
public:
  struct node {
    long page_id{-1};
    long count{0};
    
    long keys[BTREE_ORDER + 1];
    T data[BTREE_ORDER + 1];
    long children[BTREE_ORDER + 2];

    node(long page_id) : page_id{page_id} {
      count = 0;
      for (int i = 0; i < BTREE_ORDER + 2; i++) {
        children[i] = 0;
        keys[i] = 0;
      }
    }

    void insert_in_node(int pos, const T &value, long key, bool isLeaf=false) {
      int j = count;
      while (j > pos) {
        data[j] = data[j - 1];
        keys[j] = keys[j - 1];
        children[j + 1] = children[j];
        j--;
      }
      keys[j] = key;
      if (isLeaf) data[j] = value; // store data only if is leaf
      
      children[j + 1] = children[j];

      count++;
    }

    bool is_overflow() { return count > BTREE_ORDER; }
  };

  struct Metadata {
    long root_id{1};
    long count{0};
  } header;

  enum state {
    BT_OVERFLOW,
    BT_UNDERFLOW,
    NORMAL,
  };

private:
  std::shared_ptr<pagemanager> pm;

public:
  btree(std::shared_ptr<pagemanager> pm) : pm{pm} {

    node root{header.root_id};
    pm->save(root.page_id, root);

    header.count++;

    pm->save(0, header);
  }

  node new_node() {
    header.count++;
    node ret{header.count};
    pm->save(0, header);
    return ret;
  }

  node read_node(long page_id) {
    node n{-1};
    pm->recover(page_id, n);
    return n;
  }

  bool write_node(long page_id, node n) { pm->save(page_id, n); }

  void insert(const T &value, long key) {
    node root = read_node(header.root_id);
    int state = insert(root, value, key);

    if (state == BT_OVERFLOW) {
      split_root();
    }
  }

  int insert(node &ptr, const T &value, long key) {
    int pos = 0;
    while (pos < ptr.count && ptr.keys[pos] < value) {
      pos++;
    }
    if (ptr.children[pos] != 0) {
      long page_id = ptr.children[pos];
      node child = read_node(page_id);
      int state = insert(child, value, key);
      if (state == BT_OVERFLOW) {
        split(ptr, pos);
      }
    } else { // is leaf
      ptr.insert_in_node(pos, value, key, ptr.children[pos] == 0);
      write_node(ptr.page_id, ptr);
    }
    return ptr.is_overflow() ? BT_OVERFLOW : NORMAL;
  }

  void split(node &parent, int pos) {
    node ptr = this->read_node(parent.children[pos]);
    node left = this->new_node();
    node right = this->new_node();

    int iter = 0;
    int i;
    for (i = 0; iter < BTREE_ORDER / 2; i++) {
      left.children[i] = ptr.children[iter];
      left.keys[i] = ptr.keys[iter];
      if (ptr.children[0]==0) std::swap(left.data[i], ptr.data[iter]); // ptr is leaf
      left.count++;
      iter++;
    }
    left.children[i] = ptr.children[iter];

    parent.insert_in_node(pos, ptr.data[iter], ptr.keys[iter]); // will only insert key
                                                                // (parent will never be a leaf)
    if (ptr.children[0]!=0) iter++; // skip middle element only if intermediate node

    for (i = 0; iter < BTREE_ORDER + 1; i++) {
      right.children[i] = ptr.children[iter];
      right.keys[i] = ptr.keys[iter];
      if (ptr.children[0]==0) std::swap(right.data[i], ptr.data[iter]); // ptr is leaf
      right.count++;

      iter++;
    }
    right.children[i] = ptr.children[iter];

    parent.children[pos] = left.page_id;
    parent.children[pos + 1] = right.page_id;

    write_node(parent.page_id, parent);
    write_node(left.page_id, left);
    write_node(right.page_id, right);
  }

  void split_root() {
    node ptr = this->read_node(this->header.root_id);
    node left = this->new_node();
    node right = this->new_node();

    int pos = 0;
    int iter = 0;
    int i;
    for (i = 0; iter < BTREE_ORDER / 2; i++) {
      left.children[i] = ptr.children[iter];
      if (ptr.children[0]==0) std::swap(left.data[i], ptr.data[iter]); // ptr (root) is leaf
      left.keys[i] = ptr.keys[iter];
      left.count++;
      iter++;
    }
    left.children[i] = ptr.children[iter];

    if (ptr.children[0]!=0) iter++; // skip middle element only if intermediate node

    for (i = 0; iter < BTREE_ORDER + 1; i++) {
      right.children[i] = ptr.children[iter];
      if (ptr.children[0]==0) std::swap(right.data[i], ptr.data[iter]); // ptr (root) is leaf
      right.keys[i] = ptr.keys[iter];
      right.count++;
      iter++;
    }
    right.children[i] = ptr.children[iter];

    ptr.children[pos] = left.page_id;
    ptr.keys[0] = ptr.keys[BTREE_ORDER / 2]; // store key, not data
    ptr.children[pos + 1] = right.page_id;
    ptr.count = 1;

    write_node(ptr.page_id, ptr);
    write_node(left.page_id, left);
    write_node(right.page_id, right);
  }

  bool find(const T &value) { return false; }

  void print() {
    node root = read_node(header.root_id);
    print(root, 0);
    std::cout << "________________________\n";
  }

  void print(node &ptr, int level) {
    int i;
    for (i = ptr.count - 1; i >= 0; i--) {
      if (ptr.children[i + 1]) {
        node child = read_node(ptr.children[i + 1]);
        print(child, level + 1);
      }

      for (int k = 0; k < level; k++) {
        std::cout << "    ";
      }
      std::cout << (char)ptr.data[i] << ptr.keys[i] << "\n";
    }
    if (ptr.children[i + 1]) {
      node child = read_node(ptr.children[i + 1]);
      print(child, level + 1);
    }
  }
};

} // namespace disk
} // namespace utec