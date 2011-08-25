/*
 * This file is part of OpenObjectStore OOS.
 *
 * OpenObjectStore OOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenObjectStore OOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenObjectStore OOS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "object/object_store.hpp"
#include "object/object_atomizer.hpp"
#include "object/object_list.hpp"
#include "object/prototype_node.hpp"

#include <tr1/functional>
#include <memory>
#include <iostream>
#include <typeinfo>
#include <algorithm>
#include <stack>

using std::tr1::placeholders::_1;

namespace oos {

class equal_type : public std::unary_function<const prototype_node*, bool> {
public:
  explicit equal_type(const std::string &type) : type_(type) {}

  bool operator() (const prototype_node *x) const {
    return x->type == type_;
  }
private:
  const std::string &type_;
};

class object_actor : public object_atomizer
{
public:
  virtual ~object_actor() {}
  
protected:
  std::stack<object*> object_stack_;
};

class object_creator : public object_actor
{
public:
  object_creator(object_store &ostore) : ostore_(ostore) {}
  virtual ~object_creator() {}

  virtual void read_object(const char*, base_object_ptr &x)
  {
    if (!x.is_reference()) {
      // create object
      object *o = ostore_.create(x.type());
      ostore_.insert(o);
      x.reset(o);
      object_stack_.push(x.ptr());
      x.ptr()->read_from(this);
      object_stack_.pop();
    }
  }
  virtual void read_object_list(const char*, object_list_base &x)
  {
    ostore_.insert_object_list(x);
  }
private:
  object_store &ostore_;
};

class object_deleter : public object_actor
{
public:
  object_deleter(object_store &ostore) : ostore_(ostore) {}
  virtual ~object_deleter() {}

  virtual void read_object(const char*, base_object_ptr &x)
  {
    if (!x.is_reference() && x.ptr()) {
      ostore_.remove_object(x.ptr());
    }
  }
  virtual void read_object_list(const char*, object_list_base &)
  {
  }
private:
  object_store &ostore_;
};

object_store::object_store()
  : root_(new prototype_node(new object_producer<object>, "OBJECT"))
  , id_(0)
  , first_(new object_proxy(NULL, this))
  , last_(new object_proxy(NULL, this))
{
  prototype_node_name_map_.insert(std::make_pair("OBJECT", root_.get()));
  prototype_node_type_map_.insert(std::make_pair(root_->producer->classname(), root_.get()));
  // set marker for root element
  root_->op_first = first_;
  root_->op_marker = last_;
  root_->op_last = last_;
  root_->op_first->next = root_->op_last;
  root_->op_last->prev = root_->op_first;
}

void delete_object_proxy(object_proxy *op)
{
  delete op;
}

object_store::~object_store()
{
  delete first_;
  delete last_;
  // delete all deleted object_proxys
  std::for_each(deleted_object_proxy_list_.begin(), deleted_object_proxy_list_.end(), delete_object_proxy);
}

bool
object_store::insert_prototype(object_base_producer *producer, const char *type, const char *parent)
{
  // set node to root node
  prototype_node *parent_node;
  if (parent) {
    // parent type name is set search parent node
    t_prototype_node_map::iterator i = prototype_node_name_map_.find(parent);
    if (i == prototype_node_name_map_.end()) {
      //throw new object_exception("couldn't find parent prototype");
      return false;
    }
    parent_node = i->second;
  }
  // find prototype node 'type' starting from node
  t_prototype_node_map::iterator i = prototype_node_name_map_.find(type);
  if (i != prototype_node_name_map_.end()) {
    //throw new object_exception("prototype already exists");
    return false;
  }
  prototype_node *node = new prototype_node(producer, type);
  // append as child to parent prototype node
  parent_node->insert(node);
  // store prototype in map
  prototype_node_name_map_.insert(std::make_pair(type, node));
  prototype_node_type_map_.insert(std::make_pair(producer->classname(), node));
  // return success
  return true;
}

bool object_store::remove_prototype(const char *type)
{
  t_prototype_node_map::iterator i = prototype_node_name_map_.find(type);
  if (i == prototype_node_name_map_.end()) {
    //throw new object_exception("couldn't find prototype");
    return false;
  }
  if (!i->second->parent) {
    // now parent
    return false;
  }
  t_prototype_node_map::iterator j = prototype_node_type_map_.find(i->second->producer->classname());
  if (j == prototype_node_type_map_.end()) {
		// couldn't fnd prototype in type map
		// throw exception
		return false;
	}

  std::cout << "removing prototye [" << type << "]\n";
  std::cout.flush();
  /*
	prototype_node *node = i->second->next_node(i->second);
  while (node && node != i->second->last->prev) {
		t_prototype_node_map::iterator ii = prototype_node_name_map_.find(node->type);
		if (ii != prototype_node_name_map_.end()) {
			prototype_node_name_map_.erase(ii);
		}
		t_prototype_node_map::iterator jj = prototype_node_type_map_.find(node->producer->classname());
		if (jj != prototype_node_type_map_.end()) {
			prototype_node_type_map_.erase(jj);
		}
		node = node->next_node();
  }
  */
  // remove (and delete) from tree (deletes subsequently all child nodes
  // for each child call remove_prototype(child);
  while (i->second->first->next != i->second->last.get()) {
    prototype_node *node = i->second->first->next;
    remove_prototype(node->type.c_str());
  }
  // and objects they're containing 
  i->second->clear();
  // unlink node
  i->second->unlink();
  // delete node
  delete i->second;
  // erase node from maps
  prototype_node_name_map_.erase(i);
  prototype_node_type_map_.erase(j);
  return true;
}

void object_store::clear()
{
  while (root_->first->next != root_->last.get()) {
    prototype_node *node = root_->first->next;
    remove_prototype(node->type.c_str());
    //delete node;
  }  
}

int depth(prototype_node *node)
{
  int d = 0;
  while (node->parent) {
    node = node->parent;
    ++d;
  }
  return d;
}

void object_store::dump_prototypes(std::ostream &out) const
{
  prototype_node *node = root_.get();
//  out << "dumping prototype tree:\n";
  out << "digraph G {\n";
  out << "\tgraph [fontsize=14]\n";
	out << "\tnode [color=\"#0c0c0c\", fillcolor=\"#dd5555\", shape=record, style=\"rounded,filled\", fontname=\"Verdana-Bold\"]\n";
	out << "\tedge [color=\"#0c0c0c\"]\n";
  do {
    int d = depth(node);
    for (int i = 0; i < d; ++i) out << " ";
    out << *node;
    node = node->next_node();
  } while (node);
  /*
  object_proxy *i = first_;
  while (i != last_) {
    out << "\t" << i << "[label=\"{" << i << "}\"]\n";
    i = i->next;
  }
  */
  out << "}" << std::endl;
}

void object_store::dump_objects(std::ostream &out) const
{
  out << "dumping all objects\n";

  object_proxy *op = first_;
  while (op) {
    out << "[" << op << "] (";
    if (op->obj) {
      out << *op->obj << " prev [" << op->prev->obj << "] next [" << op->next->obj << "])\n";
    } else {
      out << "object 0)\n";
    }
    op = op->next;
  }
}

object* object_store::create(const char *type) const
{
  t_prototype_node_map::const_iterator i = prototype_node_name_map_.find(type);
  if (i == prototype_node_name_map_.end()) {
    // try it with the type map
    i = prototype_node_type_map_.find(type);
    if (i == prototype_node_type_map_.end()) {
      //throw new object_exception("couldn't find prototype");
      return NULL;
    }
  }
	return i->second->producer->create();
}

void object_store::mark_modified(object_proxy *oproxy)
{
  std::for_each(observer_list_.begin(), observer_list_.end(), std::tr1::bind(&object_observer::on_update, _1, oproxy->obj));
}

void object_store::register_observer(object_observer *observer)
{
  if (std::find(observer_list_.begin(), observer_list_.end(), observer) == observer_list_.end()) {
    observer_list_.push_back(observer);
  }
}

void object_store::unregister_observer(object_observer *observer)
{
  t_observer_list::iterator i = std::find(observer_list_.begin(), observer_list_.end(), observer);
  if (i != observer_list_.end()) {
    delete *i;
    observer_list_.erase(i);
  }
}

object* object_store::insert_object(object *o)
{
  // find type in tree
  if (!o) {
    // throw exception
    return NULL;
  }
  // find prototype node
  t_prototype_node_map::iterator i = prototype_node_type_map_.find(typeid(*o).name());
  if (i == prototype_node_type_map_.end()) {
    // raise exception
    //throw new object_exception("couldn't insert element of type [" + o->type() + "]\n");
    return NULL;
  }
  prototype_node *node = i->second;
  // retrieve and set new unique number into object
  //object->id(UniqueProducer::instance().number("default"));
  o->id(++id_);

  // insert new element node
  object_proxy *oproxy = new object_proxy(o, this);
  
  // check count of object in subtree
  if (node->count >= 2) {
    // there are more than two objects (normal case)
    // insert before last last
    //std::cout << "more than two elements: inserting " << *o << " before second last (" << *node->op_marker->prev->obj << ")\n";
    node->op_marker->prev->insert(oproxy);
  } else if (node->count == 1) {
    // there is one object in subtree
    // insert as first; adjust "left" marker
    /*if (node->op_marker->prev->obj) {
      std::cout << "one element in list: inserting " << *o << " as first (before: " << *node->op_marker->prev->obj << ")\n";
    } else {
      std::cout << "one element in list: inserting " << *o << " as first (before: [0])\n";
    }*/
    node->op_marker->prev->insert(oproxy);
    node->adjust_left_marker(oproxy->next, oproxy);
  } else /* if (node->count == 0) */ {
    // there is no object in subtree
    // insert as last; adjust "right" marker
    /*if (node->op_marker->obj) {
      std::cout << "list is empty: inserting " << *o << " as last before " << *node->op_marker->obj << "\n";
    } else {
      std::cout << "list is empty: inserting " << *o << " as last before [0]\n";
    }*/
    node->op_marker->insert(oproxy);
    node->adjust_left_marker(oproxy->next, oproxy);
    node->adjust_right_marker(oproxy->prev, oproxy);
  }
  // create object
  object_creator oc(*this);
  o->read_from(&oc);
  // notify observer
  std::for_each(observer_list_.begin(), observer_list_.end(), std::tr1::bind(&object_observer::on_insert, _1, o));
  // dump objects
  //std::cout << std::endl;
/*  dump_objects(std::cout);
  std::cout << std::endl;*/
  // adjust size
  ++node->count;
  // insert element into hash map
  //  objectmap_[object->id()] = object;
  // set this into persistent object
  o->proxy_ = oproxy;
  return o;
}

bool object_store::remove_object(object *o)
{
  // find prototype node
  t_prototype_node_map::iterator i = prototype_node_type_map_.find(typeid(*o).name());
  if (i == prototype_node_type_map_.end()) {
    // raise exception
    //throw new object_exception("couldn't insert element of type [" + o->type() + "]\n");
    return false;
  }
  prototype_node *node = i->second;

  if (o->proxy_ == node->op_first->next) {
    // adjust left marker
    //std::cout << "remove: object proxy is left marker " << *o << " before second last (" << *node->op_marker->prev->obj << ")\n";
    node->adjust_left_marker(node->op_first->next, node->op_first->next->next);
    //adjust_left_marker(node, node->op_first->next->next);
  }
  if (o->proxy_ == node->op_marker->prev) {
    // adjust right marker
    node->adjust_right_marker(o->proxy_, node->op_marker->prev->prev);
    //adjust_right_marker(node, o->proxy_, node->op_marker->prev->prev);
  }
  // unlink object_proxy
  o->proxy_->remove();

  // call object deleter for object
  object_deleter deleter(*this);
  o->read_from(&deleter);
  // notify observer
  std::for_each(observer_list_.begin(), observer_list_.end(), std::tr1::bind(&object_observer::on_delete, _1, o));
  // if object was last object in list of prototype node
  // adjust prototype node last
  // if object was last object (now empty)
  // adjust prototype node first, marker and last

  // mark object proxy as deleted
  // set object in object_proxy to null
  object_proxy *op = o->proxy_;
  delete o;
  op->obj = NULL;
  deleted_object_proxy_list_.push_back(op);
  // return true
  return true;
}

bool object_store::insert_object_list(object_list_base &olb)
{
  olb.ostore_ = this;
  insert_object(olb.first_obj_);
  insert_object(olb.last_obj_);
  return true;
}

}
