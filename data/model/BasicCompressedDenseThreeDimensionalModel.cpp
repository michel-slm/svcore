/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "BasicCompressedDenseThreeDimensionalModel.h"

#include "base/LogRange.h"

#include <QTextStream>
#include <QStringList>
#include <QReadLocker>
#include <QWriteLocker>

#include <iostream>

#include <cmath>
#include <cassert>

using std::vector;

#include "system/System.h"

BasicCompressedDenseThreeDimensionalModel::BasicCompressedDenseThreeDimensionalModel(sv_samplerate_t sampleRate,
                                                                                     int resolution,
                                                                                     int yBinCount,
                                                                                     bool notifyOnAdd) :
    m_startFrame(0),
    m_sampleRate(sampleRate),
    m_resolution(resolution),
    m_yBinCount(yBinCount),
    m_minimum(0.0),
    m_maximum(0.0),
    m_haveExtents(false),
    m_notifyOnAdd(notifyOnAdd),
    m_sinceLastNotifyMin(-1),
    m_sinceLastNotifyMax(-1),
    m_completion(100)
{
}    

bool
BasicCompressedDenseThreeDimensionalModel::isOK() const
{
    return true;
}

bool
BasicCompressedDenseThreeDimensionalModel::isReady(int *completion) const
{
    if (completion) *completion = getCompletion();
    return true;
}

sv_samplerate_t
BasicCompressedDenseThreeDimensionalModel::getSampleRate() const
{
    return m_sampleRate;
}

sv_frame_t
BasicCompressedDenseThreeDimensionalModel::getStartFrame() const
{
    return m_startFrame;
}

void
BasicCompressedDenseThreeDimensionalModel::setStartFrame(sv_frame_t f)
{
    m_startFrame = f; 
}

sv_frame_t
BasicCompressedDenseThreeDimensionalModel::getTrueEndFrame() const
{
    return m_resolution * m_data.size() + (m_resolution - 1);
}

int
BasicCompressedDenseThreeDimensionalModel::getResolution() const
{
    return m_resolution;
}

void
BasicCompressedDenseThreeDimensionalModel::setResolution(int sz)
{
    m_resolution = sz;
}

int
BasicCompressedDenseThreeDimensionalModel::getWidth() const
{
    return int(m_data.size());
}

int
BasicCompressedDenseThreeDimensionalModel::getHeight() const
{
    return m_yBinCount;
}

void
BasicCompressedDenseThreeDimensionalModel::setHeight(int sz)
{
    m_yBinCount = sz;
}

float
BasicCompressedDenseThreeDimensionalModel::getMinimumLevel() const
{
    return m_minimum;
}

void
BasicCompressedDenseThreeDimensionalModel::setMinimumLevel(float level)
{
    m_minimum = level;
}

float
BasicCompressedDenseThreeDimensionalModel::getMaximumLevel() const
{
    return m_maximum;
}

void
BasicCompressedDenseThreeDimensionalModel::setMaximumLevel(float level)
{
    m_maximum = level;
}

BasicCompressedDenseThreeDimensionalModel::Column
BasicCompressedDenseThreeDimensionalModel::getColumn(int index) const
{
    QReadLocker locker(&m_lock);
    if (in_range_for(m_data, index)) return expandAndRetrieve(index);
    else return Column();
}

float
BasicCompressedDenseThreeDimensionalModel::getValueAt(int index, int n) const
{
    Column c = getColumn(index);
    if (in_range_for(c, n)) return c.at(n);
    return m_minimum;
}

QString
BasicCompressedDenseThreeDimensionalModel::getValueUnit() const
{
    return m_unit;
}

void
BasicCompressedDenseThreeDimensionalModel::setValueUnit(QString unit)
{
    m_unit = unit;
}

//static int given = 0, stored = 0;

void
BasicCompressedDenseThreeDimensionalModel::truncateAndStore(int index,
                                                     const Column &values)
{
    assert(in_range_for(m_data, index));

    //cout << "truncateAndStore(" << index << ", " << values.size() << ")" << endl;

    // The default case is to store the entire column at m_data[index]
    // and place 0 at m_trunc[index] to indicate that it has not been
    // truncated.  We only do clever stuff if one of the clever-stuff
    // tests works out.

    m_trunc[index] = 0;
    if (index == 0 ||
        int(values.size()) != m_yBinCount) {
//        given += values.size();
//        stored += values.size();
        m_data[index] = values;
        return;
    }

    // Maximum distance between a column and the one we refer to as
    // the source of its truncated values.  Limited by having to fit
    // in a signed char, but in any case small values are usually
    // better
    static int maxdist = 6;

    bool known = false; // do we know whether to truncate at top or bottom?
    bool top = false;   // if we do know, will we truncate at top?

    // If the previous column is not truncated, then it is the only
    // candidate for comparison.  If it is truncated, then the column
    // that it refers to is the only candidate.  Either way, we only
    // have one possible column to compare against here, and we are
    // being careful to ensure it is not a truncated one (to avoid
    // doing more work recursively when uncompressing).
    int tdist = 1;
    int ptrunc = m_trunc[index-1];
    if (ptrunc < 0) {
        top = false;
        known = true;
        tdist = -ptrunc + 1;
    } else if (ptrunc > 0) {
        top = true;
        known = true;
        tdist = ptrunc + 1;
    }

    Column p = expandAndRetrieve(index - tdist);
    int h = m_yBinCount;

    if (int(p.size()) == h && tdist <= maxdist) {

        int bcount = 0, tcount = 0;
        if (!known || !top) {
            // count how many identical values there are at the bottom
            for (int i = 0; i < h; ++i) {
                if (values.at(i) == p.at(i)) ++bcount;
                else break;
            }
        }
        if (!known || top) {
            // count how many identical values there are at the top
            for (int i = h; i > 0; --i) {
                if (values.at(i-1) == p.at(i-1)) ++tcount;
                else break;
            }
        }
        if (!known) top = (tcount > bcount);

        int limit = h / 4; // don't bother unless we have at least this many
        if ((top ? tcount : bcount) > limit) {
        
            if (!top) {
                // create a new column with h - bcount values from bcount up
                Column tcol(h - bcount);
//                given += values.size();
//                stored += h - bcount;
                for (int i = bcount; i < h; ++i) {
                    tcol[i - bcount] = values.at(i);
                }
                m_data[index] = tcol;
                m_trunc[index] = (signed char)(-tdist);
                return;
            } else {
                // create a new column with h - tcount values from 0 up
                Column tcol(h - tcount);
//                given += values.size();
//                stored += h - tcount;
                for (int i = 0; i < h - tcount; ++i) {
                    tcol[i] = values.at(i);
                }
                m_data[index] = tcol;
                m_trunc[index] = (signed char)(tdist);
                return;
            }
        }
    }                

//    given += values.size();
//    stored += values.size();
//    cout << "given: " << given << ", stored: " << stored << " (" 
//              << ((float(stored) / float(given)) * 100.f) << "%)" << endl;

    // default case if nothing wacky worked out
    m_data[index] = values;
    return;
}

BasicCompressedDenseThreeDimensionalModel::Column
BasicCompressedDenseThreeDimensionalModel::rightHeight(const Column &c) const
{
    if (int(c.size()) == m_yBinCount) return c;
    else {
        Column cc(c);
        cc.resize(m_yBinCount, 0.0);
        return cc;
    }
}

BasicCompressedDenseThreeDimensionalModel::Column
BasicCompressedDenseThreeDimensionalModel::expandAndRetrieve(int index) const
{
    // See comment above m_trunc declaration in header

    assert(index >= 0 && index < int(m_data.size()));
    Column c = m_data.at(index);
    if (index == 0) {
        return rightHeight(c);
    }
    int trunc = (int)m_trunc[index];
    if (trunc == 0) {
        return rightHeight(c);
    }
    bool top = true;
    int tdist = trunc;
    if (trunc < 0) { top = false; tdist = -trunc; }
    Column p = expandAndRetrieve(index - tdist);
    int psize = int(p.size()), csize = int(c.size());
    if (psize != m_yBinCount) {
        cerr << "WARNING: BasicCompressedDenseThreeDimensionalModel::expandAndRetrieve: Trying to expand from incorrectly sized column" << endl;
    }
    if (top) {
        for (int i = csize; i < psize; ++i) {
            c.push_back(p.at(i));
        }
    } else {
        Column cc(psize);
        for (int i = 0; i < psize - csize; ++i) {
            cc[i] = p.at(i);
        }
        for (int i = 0; i < csize; ++i) {
            cc[i + (psize - csize)] = c.at(i);
        }
        return cc;
    }
    return c;
}

void
BasicCompressedDenseThreeDimensionalModel::setColumn(int index,
                                              const Column &values)
{
    QWriteLocker locker(&m_lock);

    while (index >= int(m_data.size())) {
        m_data.push_back(Column());
        m_trunc.push_back(0);
    }

    bool allChange = false;

    for (int i = 0; in_range_for(values, i); ++i) {
        float value = values[i];
        if (ISNAN(value) || ISINF(value)) {
            continue;
        }
        if (!m_haveExtents || value < m_minimum) {
            m_minimum = value;
            allChange = true;
        }
        if (!m_haveExtents || value > m_maximum) {
            m_maximum = value;
            allChange = true;
        }
        m_haveExtents = true;
    }

    truncateAndStore(index, values);

//    assert(values == expandAndRetrieve(index));

    sv_frame_t windowStart = index;
    windowStart *= m_resolution;

    if (m_notifyOnAdd) {
        if (allChange) {
            emit modelChanged(getId());
        } else {
            emit modelChangedWithin(getId(),
                                    windowStart, windowStart + m_resolution);
        }
    } else {
        if (allChange) {
            m_sinceLastNotifyMin = -1;
            m_sinceLastNotifyMax = -1;
            emit modelChanged(getId());
        } else {
            if (m_sinceLastNotifyMin == -1 ||
                windowStart < m_sinceLastNotifyMin) {
                m_sinceLastNotifyMin = windowStart;
            }
            if (m_sinceLastNotifyMax == -1 ||
                windowStart > m_sinceLastNotifyMax) {
                m_sinceLastNotifyMax = windowStart;
            }
        }
    }
}

QString
BasicCompressedDenseThreeDimensionalModel::getBinName(int n) const
{
    if (n >= 0 && (int)m_binNames.size() > n) return m_binNames[n];
    else return "";
}

void
BasicCompressedDenseThreeDimensionalModel::setBinName(int n, QString name)
{
    while ((int)m_binNames.size() <= n) m_binNames.push_back("");
    m_binNames[n] = name;
    emit modelChanged(getId());
}

void
BasicCompressedDenseThreeDimensionalModel::setBinNames(std::vector<QString> names)
{
    m_binNames = names;
    emit modelChanged(getId());
}

bool
BasicCompressedDenseThreeDimensionalModel::hasBinValues() const
{
    return !m_binValues.empty();
}

float
BasicCompressedDenseThreeDimensionalModel::getBinValue(int n) const
{
    if (n < (int)m_binValues.size()) return m_binValues[n];
    else return 0.f;
}

void
BasicCompressedDenseThreeDimensionalModel::setBinValues(std::vector<float> values)
{
    m_binValues = values;
}

QString
BasicCompressedDenseThreeDimensionalModel::getBinValueUnit() const
{
    return m_binValueUnit;
}

void
BasicCompressedDenseThreeDimensionalModel::setBinValueUnit(QString unit)
{
    m_binValueUnit = unit;
}

bool
BasicCompressedDenseThreeDimensionalModel::shouldUseLogValueScale() const
{
    QReadLocker locker(&m_lock);

    vector<double> sample;
    vector<int> n;
    
    for (int i = 0; i < 10; ++i) {
        int index = i * 10;
        if (in_range_for(m_data, index)) {
            const Column &c = m_data.at(index);
            while (c.size() > sample.size()) {
                sample.push_back(0.0);
                n.push_back(0);
            }
            for (int j = 0; in_range_for(c, j); ++j) {
                sample[j] += c.at(j);
                ++n[j];
            }
        }
    }

    if (sample.empty()) return false;
    for (decltype(sample)::size_type j = 0; j < sample.size(); ++j) {
        if (n[j]) sample[j] /= n[j];
    }
    
    return LogRange::shouldUseLogScale(sample);
}

void
BasicCompressedDenseThreeDimensionalModel::setCompletion(int completion, bool update)
{
    if (m_completion != completion) {
        m_completion = completion;

        if (completion == 100) {

            m_notifyOnAdd = true; // henceforth
            emit modelChanged(getId());
            emit ready(getId());

        } else if (!m_notifyOnAdd) {

            if (update &&
                m_sinceLastNotifyMin >= 0 &&
                m_sinceLastNotifyMax >= 0) {
                emit modelChangedWithin(getId(),
                                        m_sinceLastNotifyMin,
                                        m_sinceLastNotifyMax + m_resolution);
                m_sinceLastNotifyMin = m_sinceLastNotifyMax = -1;
            } else {
                emit completionChanged(getId());
            }
        } else {
            emit completionChanged(getId());
        }            
    }
}

int
BasicCompressedDenseThreeDimensionalModel::getCompletion() const
{
    return m_completion;
}

QVector<QString>
BasicCompressedDenseThreeDimensionalModel::getStringExportHeaders(DataExportOptions)
    const
{
    QVector<QString> sv;
    for (int i = 0; i < m_yBinCount; ++i) {
        sv.push_back(QString("Bin%1").arg(i+1));
    }
    return sv;
}    
    
QVector<QVector<QString>>
BasicCompressedDenseThreeDimensionalModel::toStringExportRows(DataExportOptions,
                                                              sv_frame_t startFrame,
                                                              sv_frame_t duration)
    const
{
    QReadLocker locker(&m_lock);

    QVector<QVector<QString>> rows;

    for (int i = 0; in_range_for(m_data, i); ++i) {
        Column c = getColumn(i);
        sv_frame_t fr = m_startFrame + i * m_resolution;
        if (fr >= startFrame && fr < startFrame + duration) {
            QVector<QString> row;
            for (int j = 0; in_range_for(c, j); ++j) {
                row << QString("%1").arg(c.at(j));
            }
            rows.push_back(row);
        }
    }
    
    return rows;
}

void
BasicCompressedDenseThreeDimensionalModel::toXml(QTextStream &out,
                                          QString indent,
                                          QString extraAttributes) const
{
    QReadLocker locker(&m_lock);

    // For historical reasons we read and write "resolution" as "windowSize".

    // Our dataset doesn't have its own export ID, we just use
    // ours. Actually any model could do that, since datasets aren't
    // in the same id-space as models when re-read

    SVDEBUG << "BasicCompressedDenseThreeDimensionalModel::toXml" << endl;

    Model::toXml
        (out, indent,
         QString("type=\"dense\" dimensions=\"3\" windowSize=\"%1\" yBinCount=\"%2\" minimum=\"%3\" maximum=\"%4\" dataset=\"%5\" startFrame=\"%6\" %7")
         .arg(m_resolution)
         .arg(m_yBinCount)
         .arg(m_minimum)
         .arg(m_maximum)
         .arg(getExportId())
         .arg(m_startFrame)
         .arg(extraAttributes));

    out << indent;
    out << QString("<dataset id=\"%1\" dimensions=\"3\" separator=\" \">\n")
        .arg(getExportId());

    for (int i = 0; in_range_for(m_binNames, i); ++i) {
        if (m_binNames[i] != "") {
            out << indent + "  ";
            out << QString("<bin number=\"%1\" name=\"%2\"/>\n")
                .arg(i).arg(m_binNames[i]);
        }
    }

    for (int i = 0; in_range_for(m_data, i); ++i) {
        Column c = getColumn(i);
        out << indent + "  ";
        out << QString("<row n=\"%1\">").arg(i);
        for (int j = 0; in_range_for(c, j); ++j) {
            if (j > 0) out << " ";
            out << c.at(j);
        }
        out << QString("</row>\n");
        out.flush();
    }

    out << indent + "</dataset>\n";
}


