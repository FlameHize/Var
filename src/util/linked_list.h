// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VAR_UTIL_LINKED_LIST_H
#define VAR_UTIL_LINKED_LIST_H

#include "net/base/noncopyable.h"

// Simple LinkedList type. (See the Q&A section to understand how this
// differs from std::list).
//
// To use, start by declaring the class which will be contained in the linked
// list, as extending LinkNode (this gives it next/previous pointers).
//
//   class MyNodeType : public LinkNode<MyNodeType> {
//     ...
//   };
//
// Next, to keep track of the list's head/tail, use a LinkedList instance:
//
//   LinkedList<MyNodeType> list;
//
// To add elements to the list, use any of LinkedList::Append,
// LinkNode::InsertBefore, or LinkNode::InsertAfter:
//
//   LinkNode<MyNodeType>* n1 = ...;
//   LinkNode<MyNodeType>* n2 = ...;
//   LinkNode<MyNodeType>* n3 = ...;
//
//   list.Append(n1);
//   list.Append(n3);
//   n2->InsertBefore(n3);
//
// Lastly, to iterate through the linked list forwards:
//
//   for (LinkNode<MyNodeType>* node = list.head();
//        node != list.end();
//        node = node->next()) {
//     MyNodeType* value = node->value();
//     ...
//   }
//
// Or to iterate the linked list backwards:
//
//   for (LinkNode<MyNodeType>* node = list.tail();
//        node != list.end();
//        node = node->previous()) {
//     MyNodeType* value = node->value();
//     ...
//   }
//
// Questions and Answers:
//
// Q. Should I use std::list or butil::LinkedList?
//
// A. The main reason to use butil::LinkedList over std::list is
//    performance. If you don't care about the performance differences
//    then use an STL container, as it makes for better code readability.
//
//    Comparing the performance of butil::LinkedList<T> to std::list<T*>:
//
//    * Erasing an element of type T* from butil::LinkedList<T> is
//      an O(1) operation. Whereas for std::list<T*> it is O(n).
//      That is because with std::list<T*> you must obtain an
//      iterator to the T* element before you can call erase(iterator).
//
//    * Insertion operations with butil::LinkedList<T> never require
//      heap allocations.
//
// Q. How does butil::LinkedList implementation differ from std::list?
//
// A. Doubly-linked lists are made up of nodes that contain "next" and
//    "previous" pointers that reference other nodes in the list.
//
//    With butil::LinkedList<T>, the type being inserted already reserves
//    space for the "next" and "previous" pointers (butil::LinkNode<T>*).
//    Whereas with std::list<T> the type can be anything, so the implementation
//    needs to glue on the "next" and "previous" pointers using
//    some internal node type.

namespace var {

template<typename T>
class LinkNode : public noncopyable {
public:
    // LinkNode are self-referential as default.
    LinkNode() : _previous(this), _next(this) {}
    LinkNode(LinkNode<T>* previous, LinkNode<T>* next) 
        : _previous(previous), _next(next) {}

    // Insert |this| into the linked list, before |e|.
    void InsertBefore(LinkNode<T>* e) {
        this->_next = e;
        this->_previous = e->_previous;
        e->_previous->_next = this;
        e->_previous = this;
    }

    // Insert |this| into the linked list, after |e|.
    void InsertAfter(LinkNode<T>* e) {
        this->_next = e->_next;
        this->_previous = e;
        e->_next->_previous = this;
        e->_next = this;
    }

    // Default mode: |this| is the head of first linked list, 
    // |e| is the head of the second linked list.
    // Insert |this| as a circular linked list into the linked list, before |e|.
    void InsertBeforeAsList(LinkNode<T>* e) {
        LinkNode<T>* prev = this->_previous;
        prev->_next = e;
        this->_previous = e->_previous;
        e->_previous->_next = this;
        e->_previous = prev;
    }

    // Insert |this| as a circular list into the linked list, after |e|.
    void InsertAfterAsList(LinkNode<T>* e) {
        LinkNode<T>* prev = this->_previous;
        prev->_next = e->_next;
        this->_previous = e;
        e->_next->_previous = prev;
        e->_next = this;
    }

    // Remove |this| from the linked list.
    void RemoveFromList() {
        this->_previous->_next = this->_next;
        this->_next->_previous = this->_previous;
        this->_next = this;
        this->_previous = this;
    }

    LinkNode<T>* previous() const {
        return _previous;
    }

    LinkNode<T>* next() const {
        return _next;
    }

    // Cast from the node-type to the value type.
    const T* value() const {
        return static_cast<const T*>(this);
    }

    T* value() {
        return static_cast<T*>(this);
    }

private:
    LinkNode<T>* _previous;
    LinkNode<T>* _next;
};

template<typename T>
class LinkedList : public noncopyable {
public:
    // The "root" node is self-referential, and forms the basis of a circular
    // list (root_.next() will point back to the start of the list,
    // and root_->previous() wraps around to the end of the list).
    LinkedList() {}

    // Appends |e| to the end of the linked list.
    void Append(LinkNode<T>* e) {
        e->InsertBefore(&_root);
    }

    LinkNode<T>* head() const {
        return _root.next();
    }

    LinkNode<T>* tail() const {
        return _root.previous();
    }

    const LinkNode<T>* end() const {
        return &_root;
    }

    bool empty() const {
        return head() == end();
    }

private:
    LinkNode<T> _root;
};

} // end namespace var


#endif // VAR_UTIL_LINKED_LIST_H