/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _RESIZEABLE_BITMAP_H_
#define _RESIZEABLE_BITMAP_H_

#include <vector>
#include <stdint.h>

class ResizeableBitset {

public:
    ResizeableBitset() : m_bits(0) {
    }
    ResizeableBitset(size_t size) : m_bits(new std::vector<uint8_t>), m_size(size) {
        m_bits->assign((size >> 3) + 1, 0);
    }
    ResizeableBitset(const ResizeableBitset &b) {
        m_bits = new std::vector<uint8_t>(*b.m_bits);
    }
    ResizeableBitset &operator=(const ResizeableBitset &b) {
        if (&b != this) return *this;
        delete m_bits;
        m_bits = new std::vector<uint8_t>(*b.m_bits);
        return *this;
    }
    ~ResizeableBitset() {
        delete m_bits;
    }
    
    void resize(size_t bits) { // losing all data
        if (!m_bits || bits < m_bits->size()) {
            delete m_bits;
            m_bits = new std::vector<uint8_t>;
        }
        m_bits->assign((bits >> 3) + 1, 0);
        m_size = bits;
    }
    
    bool get(size_t column) const {
        return ((*m_bits)[column >> 3]) & (1u << (column & 0x07));
    }
    
    void set(size_t column) {
        ((*m_bits)[column >> 3]) |=  (uint8_t(1) << (column & 0x07));
    }

    void reset(size_t column) {
        ((*m_bits)[column >> 3]) &= ~(uint8_t(1) << (column & 0x07));
    }

    void copy(size_t source, size_t dest) {
        get(source) ? set(dest) : reset(dest);
    }

    size_t size() const {
        return m_size;
    }
    
private:
    std::vector<uint8_t> *m_bits;
    size_t m_size;
};


#endif

