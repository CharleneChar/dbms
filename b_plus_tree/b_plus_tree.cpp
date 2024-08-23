#include "include/b_plus_tree.h"

#define DEBUG 0
#if DEBUG
#include <iostream>
#include <deque>
#include <utility>
#include <string>

using std::cout;
using std::deque;
using std::pair;
using std::string;

static void
PrintKeys(Node *node, string end = "\n") {
  for (int i {}; i < node->key_num; ++i) {
    cout << node->keys[i] << " ";
  }
  cout << end;
}

static void
PrintValues(LeafNode *leaf, string end = "\n") {
  for (int i {}; i < leaf->key_num; ++i) {
    cout << "(" << leaf->pointers[i].page_id
         << ", " << leaf->pointers[i].record_id
         << ") ";
  }
  cout << end;
}

static void
Print(Node *root) {
  if (not root) { cout << "empty\n"; return; }
  deque<pair<Node*, int>> q;
  deque<LeafNode*> l;
  int pd {-1};
  q.emplace_front(root, 0);
  while (not q.empty()) {
    auto [node, d] {q.front()}; q.pop_front();
    if (d not_eq pd) { pd = d; cout << "\n" << d << ": "; }
    else { cout << "| "; }
    PrintKeys(node, "");
    if (node->is_leaf) { l.emplace_back(static_cast<LeafNode*>(node)); }
    if (not node->is_leaf) {
      for (int i {}; i <= node->key_num; ++i) {
        q.emplace_back(static_cast<InternalNode*>(node)->children[i], d + 1);
      }
    }
  }
  cout << "\n";
  while (not l.empty()) {
    auto *leaf {l.front()}; l.pop_front();
    PrintValues(leaf, "| ");
  }
  cout << "\n";
}
#endif

/*
 * Helper function to decide whether current b+tree is empty
 */
bool BPlusTree::IsEmpty() const {
  return (not root);
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
bool
BPlusTree::GetValue(const KeyType &key, RecordPointer &result) {
  LeafNode *leaf {FindLeaf(key)};
  if (not leaf) { return false; }
  int i {};
  for (; i < leaf->key_num and leaf->keys[i] < key; ++i);
  result = leaf->pointers[i];
  return true;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * If current tree is empty, start new tree, otherwise insert into leaf Node.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
bool
BPlusTree::Insert(const KeyType &key, const RecordPointer &value) {
  // new root
  if (not root) {
    LeafNode *leaf {new LeafNode};
    leaf->key_num = 1;
    leaf->keys[0] = key;
    leaf->pointers[0] = value;
    root = leaf;
    return true;
  }
  // recursive insert from root
  Node *new_node {};
  KeyType new_key;
  if (not Insert(root, key, value, new_node, new_key)) { return false; }
  // no overflow in root
  if (not new_node) { return true; }
  // overflow in root
  InternalNode *new_root {new InternalNode};
  new_root->key_num = 1;
  new_root->keys[0] = new_key;
  new_root->children[0] = root;
  new_root->children[1] = new_node;
  root = new_root;
  return true;
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immdiately.
 * If not, User needs to first find the right leaf node as deletion target, then
 * delete entry from leaf node. Remember to deal with redistribute or merge if
 * necessary.
 */
void
BPlusTree::Remove(const KeyType &key) {
#if DEBUG
  static int d {};
  if (d == 0) { Print(root); }
#endif
  if (not FindLeaf(key)) { return; }
  // leaf root
  if (root->is_leaf) {
    LeafNode *leaf {static_cast<LeafNode*>(root)};
    int i {};
    for (; i < leaf->key_num and leaf->keys[i] < key; ++i);
    for (++i; i < leaf->key_num; ++i) {
      leaf->keys[i - 1] = leaf->keys[i];
      leaf->pointers[i - 1] = leaf->pointers[i];
    }
    if (--leaf->key_num == 0) { delete root; root = nullptr; }
#if DEBUG
    Print(root); ++d; cout << key << "\n";
#endif
    return;
  }
  vector<InternalNode*> ancestors;
  vector<int> child_indexes;
  Remove(ancestors, child_indexes, root, key);
  // underflow in child
  if (root->key_num == 0) {
    Node *new_root {static_cast<InternalNode*>(root)->children[0]};
    delete root; root = new_root;
  }
#if DEBUG
  Print(root); ++d; cout << key << "\n";
#endif
  return;
}

/*****************************************************************************
 * RANGE_SCAN
 *****************************************************************************/
/*
 * Return the values that within the given key range
 * First find the node large or equal to the key_start, then traverse the leaf
 * nodes until meet the key_end position, fetch all the records.
 */
void
BPlusTree::RangeScan(const KeyType &key_start, const KeyType &key_end,
                     vector<RecordPointer> &result) {
  result.clear();
  if (key_end < key_start) { return; }
  LeafNode *leaf {FindLeaf(key_start, true)};
  if (not leaf) { return; }
  int i {};
  for (; i < leaf->key_num and leaf->keys[i] < key_start; ++i);
  if (i == leaf->key_num) { leaf = leaf->next_leaf; i = 0; }
  while (leaf and i < leaf->key_num and leaf->keys[i] <= key_end) {
    result.emplace_back(leaf->pointers[i]);
    if (++i == leaf->key_num) { leaf = leaf->next_leaf; i = 0; }
  }
  return;
}

// private

LeafNode*
BPlusTree::FindLeaf(KeyType const &key, bool is_predecessor) {
  if (not root) { return nullptr; }
  Node *parent {}, *node {root};
  while (node and not node->is_leaf) {
    for (int i {}; i < node->key_num; ++i) {
      if (key < node->keys[i]) {
        node = static_cast<InternalNode*>(node)->children[i];
        break;
      }
    }
    if (parent == node) {
      node = static_cast<InternalNode*>(node)->children[node->key_num];
    }
    parent = node;
  }
  LeafNode *leaf {static_cast<LeafNode*>(node)};
  if (not leaf or is_predecessor) { return leaf; }
  for (int i {}; i < leaf->key_num; ++i) {
    if (key == leaf->keys[i]) { return leaf; }
  }
  return nullptr;
}

bool
BPlusTree::Insert(Node *node, KeyType const &key, RecordPointer const &value,
                  Node *&new_node, KeyType &new_key) {
  // leaf
  if (node->is_leaf) {
    LeafNode *leaf {static_cast<LeafNode*>(node)};
    return InsertInLeaf(leaf, key, value, new_node, new_key);
  }
  // internal node
  int i {};
  for (; i < node->key_num; ++i) {
    // duplicate key
    if (key == node->keys[i]) { return false; } 
    else if (key < node->keys[i]) { break; }
  }
  // recursive insertion from internal node
  InternalNode *internal_node {static_cast<InternalNode*>(node)};
  if (not Insert(internal_node->children[i], key, value, new_node, new_key)) {
    return false;
  }
  // no overflow in child
  if (not new_node) { return true; }
  // no overflow in internal node
  if (internal_node->key_num < (MAX_FANOUT - 1)) {
    int j {(internal_node->key_num)++};
    for (; j > i; --j) {
      internal_node->keys[j] = internal_node->keys[j - 1];
      internal_node->children[j + 1] = internal_node->children[j];
    }
    internal_node->keys[j] = new_key;
    internal_node->children[j + 1] = new_node;
    new_node = nullptr;
    return true;
  }
  // overflow
  KeyType keys[MAX_FANOUT];
  Node *children[MAX_FANOUT + 1];
  int j {MAX_FANOUT - 1};
  for (; j > i; --j) {
    keys[j] = internal_node->keys[j - 1];
    children[j + 1] = internal_node->children[j];
  }
  keys[j] = new_key;
  children[j + 1] = new_node;
  for (--j; j > -1; --j) {
    keys[j] = internal_node->keys[j];
    children[j + 1] = internal_node->children[j + 1];
  }
  children[0] = internal_node->children[0];
  InternalNode *new_internal_node = new InternalNode;
  internal_node->key_num = MAX_FANOUT >> 1;
  new_internal_node->key_num = MAX_FANOUT - internal_node->key_num - 1;
  for (j = 0; j < internal_node->key_num; ++j) {
    internal_node->keys[j] = keys[j];
    internal_node->children[j] = children[j];
  }
  internal_node->children[j] = children[j];
  new_key = keys[j];
  int b {++j};
  for (; j < MAX_FANOUT; ++j) {
    new_internal_node->keys[j - b] = keys[j];
    new_internal_node->children[j - b] = children[j];
  }
  new_internal_node->children[j - b] = children[j];
  new_node = new_internal_node;
  return true;
}
  
bool
BPlusTree::InsertInLeaf(LeafNode *leaf, KeyType const &key,
                        RecordPointer const &value, Node *&new_node,
                        KeyType &new_key) {
  // duplicate key
  for (int i {}; i < leaf->key_num; ++i) {
    if (key == leaf->keys[i]) { return false; }
  }
  // no overflow
  if (leaf->key_num < (MAX_FANOUT - 1)) {
    int i {(leaf->key_num)++};
    for (; i > 0; --i) {
      if (key < leaf->keys[i - 1]) {
        leaf->keys[i] = leaf->keys[i - 1];
        leaf->pointers[i] = leaf->pointers[i - 1];
      } else { break; }
    }
    leaf->keys[i] = key;
    leaf->pointers[i] = value;
    new_node = nullptr;
    return true;
  }
  // overflow
  KeyType keys[MAX_FANOUT];
  RecordPointer pointers[MAX_FANOUT];
  int i {MAX_FANOUT - 1};
  for (; i > 0; --i) {
    if (key < leaf->keys[i - 1]) {
      keys[i] = leaf->keys[i - 1];
      pointers[i] = leaf->pointers[i - 1];
    } else { break; }
  }
  keys[i] = key;
  pointers[i] = value;
  for (--i; i > -1; --i) {
    keys[i] = leaf->keys[i];
    pointers[i] = leaf->pointers[i];
  }
  LeafNode *new_leaf = new LeafNode;
  leaf->key_num = MAX_FANOUT >> 1;
  new_leaf->key_num = MAX_FANOUT - leaf->key_num;
  for (i = 0; i < leaf->key_num; ++i) {
    leaf->keys[i] = keys[i];
    leaf->pointers[i] = pointers[i];
  }
  for (int b {i}; i < MAX_FANOUT; ++i) {
    new_leaf->keys[i - b] = keys[i];
    new_leaf->pointers[i - b] = pointers[i];
  }
  // connect leaves
  new_leaf->next_leaf = leaf->next_leaf;
  if (new_leaf->next_leaf) { new_leaf->next_leaf->prev_leaf = new_leaf; }
  leaf->next_leaf = new_leaf;
  new_leaf->prev_leaf = leaf;
  new_node = new_leaf;
  new_key = new_leaf->keys[0];
  return true;
}

void
BPlusTree::Remove(vector<InternalNode*> &ancestors, vector<int> &child_indexes,
                  Node *node, KeyType const &key) {
  static constexpr int threshold {(MAX_FANOUT - 1) >> 1};
  if (node->is_leaf) {
    LeafNode *leaf {static_cast<LeafNode*>(node)};
    RemoveInLeaf(ancestors, child_indexes, leaf, key);
    return;
  }
  int i {};
  for (; i < node->key_num and node->keys[i] <= key; ++i);
  InternalNode *internal_node {static_cast<InternalNode*>(node)};
  ancestors.emplace_back(internal_node); child_indexes.emplace_back(i);
  Remove(ancestors, child_indexes, internal_node->children[i], key);
  ancestors.pop_back(); child_indexes.pop_back();
  // root
  if (internal_node == root) { return; }
  // no underflow
  if (internal_node->key_num >= threshold) { return; }
  // underflow
  InternalNode *parent {ancestors.back()};
  int child_index {child_indexes.back()};
  InternalNode *left_sibling {}, *right_sibling {};
  if (child_index > 0) {
    left_sibling = static_cast<InternalNode*>(parent->children[child_index - 1]);
  }
  if (child_index < parent->key_num) {
    right_sibling = static_cast<InternalNode*>(parent->children[child_index + 1]);
  }
  // left sibling
  if (left_sibling and (not right_sibling or
                        left_sibling->key_num >= right_sibling->key_num)) {
    // steal from left sibling
    if (left_sibling->key_num > threshold) {
      int i {(internal_node->key_num)++};
      for (; i > 0; --i) {
        internal_node->keys[i] = internal_node->keys[i - 1];
        internal_node->children[i + 1] = internal_node->children[i];
      }
      internal_node->children[1] = internal_node->children[0];
      internal_node->keys[0] = parent->keys[child_index - 1];
      int &n {left_sibling->key_num};
      internal_node->children[0] = left_sibling->children[n];
      parent->keys[child_index - 1] = left_sibling->keys[n - 1];
      --n;
      return;
    }
    // merge into left sibling
    int &n {left_sibling->key_num};
    left_sibling->keys[n] = parent->keys[child_index - 1];
    left_sibling->children[n + 1] = internal_node->children[0];
    ++n;
    for (int i {}; i < internal_node->key_num; ++n, ++i) {
      left_sibling->keys[n] = internal_node->keys[i];
      left_sibling->children[n + 1] = internal_node->children[i + 1];
    }
    delete internal_node;
    for (int i {child_index}; i < parent->key_num; ++i) {
      parent->keys[i - 1] = parent->keys[i];
      parent->children[i] = parent->children[i + 1];
    }
    --(parent->key_num);
    return;
  }
  // steal from right sibling
  if (right_sibling->key_num > threshold) {
    int &n {internal_node->key_num};
    internal_node->keys[n] = parent->keys[child_index];
    internal_node->children[n + 1] = right_sibling->children[0];
    ++n;
    parent->keys[child_index] = right_sibling->keys[0];
    int i {1};
    for (; i < right_sibling->key_num; ++i) {
      right_sibling->keys[i - 1] = right_sibling->keys[i];
      right_sibling->children[i - 1] = right_sibling->children[i];
    }
    right_sibling->children[i - 1] = right_sibling->children[i];
    --(right_sibling->key_num);
    return;
  }
  // merge from right sibling
  int &n {internal_node->key_num};
  internal_node->keys[n++] = parent->keys[child_index];
  for (i = 0; i < right_sibling->key_num; ++n, ++i) {
    internal_node->keys[n] = right_sibling->keys[i];
    internal_node->children[n] = right_sibling->children[i];
  }
  internal_node->children[n] = right_sibling->children[i];
  delete right_sibling;
  for (int i {++child_index}; i < parent->key_num; ++i) {
    parent->keys[i - 1] = parent->keys[i];
    parent->children[i] = parent->children[i + 1];
  }
  --(parent->key_num);
  return;
}

void
BPlusTree::RemoveInLeaf(vector<InternalNode*> &ancestors,
                        vector<int> &child_indexes,
                        LeafNode *leaf, KeyType const &key) {
  static constexpr int threshold {MAX_FANOUT >> 1};
  // no underflow
  if (leaf->key_num > threshold) {
    RemoveInLeafAndUpdateKeyInAncestor(ancestors, child_indexes, leaf, key);
    return;
  }
  // underflow
  InternalNode *parent {ancestors.back()};
  int child_index {child_indexes.back()};
  LeafNode *left_sibling {}, *right_sibling {};
  if (child_index > 0) {
    left_sibling = static_cast<LeafNode*>(parent->children[child_index - 1]);
  }
  if (child_index < parent->key_num) {
    right_sibling = static_cast<LeafNode*>(parent->children[child_index + 1]);
  }
  // left sibling
  if (left_sibling and (not right_sibling or
                        left_sibling->key_num >= right_sibling->key_num)) {
    // steal from left sibling
    if (left_sibling->key_num > threshold) {
      int i {leaf->key_num - 1};
      for (; i > 0 and key < leaf->keys[i]; --i);
      for (; i > 0; --i) {
        leaf->keys[i] = leaf->keys[i - 1];
        leaf->pointers[i] = leaf->pointers[i - 1];
      }
      int &n {left_sibling->key_num};
      parent->keys[child_index - 1] = leaf->keys[0] = left_sibling->keys[n - 1];
      leaf->pointers[0] = left_sibling->pointers[n - 1];
      --n;
      return;
    }
    // merge into left sibling
    for (int i {}; i < leaf->key_num; ++i) {
      if (key not_eq leaf->keys[i]) {
        int &n {left_sibling->key_num};
        left_sibling->keys[n] = leaf->keys[i];
        left_sibling->pointers[n] = leaf->pointers[i];
        ++n;
      }
    }
    left_sibling->next_leaf = leaf->next_leaf;
    if (leaf->next_leaf) { leaf->next_leaf->prev_leaf = left_sibling; }
    delete leaf;
    for (int i {child_index}; i < parent->key_num; ++i) {
      parent->keys[i - 1] = parent->keys[i];
      parent->children[i] = parent->children[i + 1];
    }
    --(parent->key_num);
    return;
  }
  // right sibling
  RemoveInLeafAndUpdateKeyInAncestor(ancestors, child_indexes, leaf, key);
  // steal from right sibling
  if (right_sibling->key_num > threshold) {
    int &n {leaf->key_num};
    leaf->keys[n] = right_sibling->keys[0];
    leaf->pointers[n] = right_sibling->pointers[0];
    ++n;
    parent->keys[child_index] = right_sibling->keys[1];
    for (int i {1}; i < right_sibling->key_num; ++i) {
      right_sibling->keys[i - 1] = right_sibling->keys[i];
      right_sibling->pointers[i - 1] = right_sibling->pointers[i];
    }
    --(right_sibling->key_num);
    return;
  }
  // merge from right sibling
  for (int i {}; i < right_sibling->key_num; ++i) {
    int &n {leaf->key_num};
    leaf->keys[n] = right_sibling->keys[i];
    leaf->pointers[n] = right_sibling->pointers[i];
    ++n;
  }
  leaf->next_leaf = right_sibling->next_leaf;
  if (right_sibling->next_leaf) { right_sibling->next_leaf->prev_leaf = leaf; }
  delete right_sibling;
  for (int i {++child_index}; i < parent->key_num; ++i) {
    parent->keys[i - 1] = parent->keys[i];
    parent->children[i] = parent->children[i + 1];
  }
  --(parent->key_num);
  return;
}

void
BPlusTree::UpdateKeyInAncestor(vector<InternalNode*> const &ancestors,
                               vector<int> const &child_indexes,
                               LeafNode *leaf) {
  int i {static_cast<int>(child_indexes.size()) - 1};
  for (; i > -1 and child_indexes[i] == 0; --i);
  if (i > -1) {
    KeyType key {(leaf->key_num > 1) ? leaf->keys[1]
                                     : leaf->next_leaf->keys[0]};
    ancestors[i]->keys[child_indexes[i] - 1] = key;
  }
  return;
}

void
BPlusTree::RemoveInLeafAndUpdateKeyInAncestor(vector<InternalNode*> &ancestors,
                                              vector<int> &child_indexes,
                                              LeafNode *leaf,
                                              KeyType const &key) {
  int i {};
  for (; i < leaf->key_num and leaf->keys[i] < key; ++i);
  if (i == 0) { UpdateKeyInAncestor(ancestors, child_indexes, leaf); }
  for (++i; i < leaf->key_num; ++i) {
    leaf->keys[i - 1] = leaf->keys[i];
    leaf->pointers[i - 1] = leaf->pointers[i];
  }
  --(leaf->key_num);
  return;
}
