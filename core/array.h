/*************************************************************************/
/*  array.h                                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#pragma once

#include "core/error_list.h"
#include "core/typedefs.h"
#include "core/forward_decls.h"

class Variant;
class ArrayPrivate;
class Object;
class StringName;

class GODOT_EXPORT Array {

    mutable ArrayPrivate *_p;
    void _ref(const Array &p_from) const;
    void _unref() const;

public:
    using ValueType = Variant;

    Variant &operator[](int p_idx);
    const Variant &operator[](int p_idx) const;

    void set(int p_idx, const Variant &p_value);
    const Variant &get(int p_idx) const;

    const Vector<Variant> &vals() const;
    operator const Vector<Variant>&() const { return vals(); }

    int size() const;
    bool empty() const;
    void clear();

    bool operator==(const Array &p_array) const;

    uint32_t hash() const;
    Array &operator=(const Array &p_array);

    void push_back(const Variant &p_value);
    void emplace_back(Variant &&p_value);
    void push_back(const Variant *entries,int count);
    _FORCE_INLINE_ void append(const Variant &p_value) { push_back(p_value); } // for python compatibility
    void resize(uint32_t p_new_size);
    void reserve(uint32_t p_new_size);

    void insert(int p_pos, const Variant &p_value);
    void remove(int p_pos);

    const Variant &front() const;
    const Variant &back() const;

    Array &sort();
    Array &sort_custom(Object *p_obj, const StringName &p_function);
    void shuffle();
    int bsearch(const Variant &p_value, bool p_before = true);
    int bsearch_custom(const Variant &p_value, Object *p_obj, const StringName &p_function, bool p_before = true);
    Array &invert();

    int find(const Variant &p_value, int p_from = 0) const;
    int rfind(const Variant &p_value, int p_from = -1) const;
    int find_last(const Variant &p_value) const;
    int count(const Variant &p_value) const;
    bool contains(const Variant &p_value) const;

    void erase(const Variant &p_value);

    void push_front(const Variant &p_value);
    Variant pop_back();
    Variant pop_front();

    Array duplicate(bool p_deep = false) const;

    Array slice(int p_begin, int p_end, int p_step = 1, bool p_deep = false) const;

    Variant min() const;
    Variant max() const;
    const void *id() const;

    Array(const Array &p_from);
    Array(Vector<Variant> &&v) noexcept;
    Array(Array &&from) noexcept {
        _p = from._p;
        from._p = nullptr;
    }

    Array();
    ~Array();
};
