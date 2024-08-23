//===----------------------------------------------------------------------===//
//
//                         Rutgers CS539 - Database System
//                         ***DO NO SHARE PUBLICLY***
//
// Identification:   include/b_plus_tree.h
//
// Copyright (c) 2023, Rutgers University
//
//===----------------------------------------------------------------------===//
#pragma once

#include <queue>
#include <string>
#include <vector>
#include "para.h"

using std::vector;

// Value structure we insert into BPlusTree
struct RecordPointer {
  int page_id;
  int record_id;
  RecordPointer() : page_id(0), record_id(0){};
  RecordPointer(int page, int record) : page_id(page), record_id(record){};
};

// BPlusTree Node
class Node {
public:
  Node(bool leaf) : key_num(0), is_leaf(leaf) {};
  bool is_leaf;
  int key_num;
  KeyType keys[MAX_FANOUT - 1];
};

// internal b+ tree node
class InternalNode : public Node {
public:
  InternalNode() : Node(false) {};
  Node *children[MAX_FANOUT];
};

class LeafNode : public Node {
public:
  LeafNode() : Node(true) {};
  RecordPointer pointers[MAX_FANOUT - 1];
  // pointer to the next/prev leaf node
  LeafNode *next_leaf = NULL;
  LeafNode *prev_leaf = NULL;
};

/**
 * Main class providing the API for the Interactive B+ Tree.
 *
 * Implementation of simple b+ tree data structure where internal pages direct
 * the search and leaf pages contain record pointers
 * (1) We only support (and test) UNIQUE key
 * (2) Support insert & remove
 * (3) Support range scan, return multiple values.
 * (4) The structure should shrink and grow dynamically
 */

class BPlusTree {
public:

  BPlusTree() = default;
  virtual ~BPlusTree() = default;

  // Returns true if this B+ tree has no keys and values
  bool IsEmpty() const;

  // Insert a key-value pair into this B+ tree.
  bool Insert(const KeyType &key, const RecordPointer &value);

  // Remove a key and its value from this B+ tree.
  void Remove(const KeyType &key);

  // return the value associated with a given key
  bool GetValue(const KeyType &key, RecordPointer &result);

  // return the values within a key range [key_start, key_end) not included key_end
  void RangeScan(const KeyType &key_start, const KeyType &key_end,
                 vector<RecordPointer> &result);

private:

  LeafNode* FindLeaf(KeyType const &key, bool is_predecessor = false);
  
  bool Insert(Node *node, KeyType const &key, RecordPointer const &value,
              Node *&new_node, KeyType &new_key);
  bool InsertInLeaf(LeafNode *node, KeyType const &key,
                    RecordPointer const &value, Node *&new_node,
                    KeyType &new_key);
  void Remove(vector<InternalNode*> &ancestors, vector<int> &child_indexes,
              Node *node, KeyType const &key);
  void RemoveInLeaf(vector<InternalNode*> &ancestors,
                    vector<int> &child_indexes,
                    LeafNode *leaf, KeyType const &key);
  void UpdateKeyInAncestor(vector<InternalNode*> const &ancestors,
                           vector<int> const &child_indexes,
                           LeafNode *leaf);
  void RemoveInLeafAndUpdateKeyInAncestor(vector<InternalNode*> &ancestors,
                                          vector<int> &child_indexes,
                                          LeafNode *leaf, KeyType const &key);

public:

  // pointer to the root node.
  Node *root {};

};
