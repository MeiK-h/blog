#ifndef BLOG_READ_BUFFER_TREE_H
#define BLOG_READ_BUFFER_TREE_H

#include <iostream>

#include "read_buffer.h"

enum {
    red,
    black,
};

struct ReadBufferNode {
    int key;
    ReadBuffer *reader;

    int color;
    ReadBufferNode *parent;
    ReadBufferNode *left;
    ReadBufferNode *right;

    ReadBufferNode(int key) {
        this->key = key;
        this->reader = new ReadBuffer(key);
        this->left = this->right = nullptr;
        this->color = red;
    }
    ~ReadBufferNode() { delete this->reader; }
};

class ReadBufferTree {
   public:
    ReadBufferTree();
    ~ReadBufferTree();
    void Insert(int);
    ReadBufferNode *Find(int);
    void Remove(int);

   private:
    ReadBufferNode *root_;
    void InsertRebalance(ReadBufferNode *);
    void RemoveRebalance(ReadBufferNode *);
    void Destroy(ReadBufferNode *);
};

#endif