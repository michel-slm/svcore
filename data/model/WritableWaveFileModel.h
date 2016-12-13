/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef WRITABLE_WAVE_FILE_MODEL_H
#define WRITABLE_WAVE_FILE_MODEL_H

#include "WaveFileModel.h"
#include "ReadOnlyWaveFileModel.h"
#include "PowerOfSqrtTwoZoomConstraint.h"

class WavFileWriter;
class WavFileReader;

class WritableWaveFileModel : public WaveFileModel
{
    Q_OBJECT

public:
    WritableWaveFileModel(sv_samplerate_t sampleRate, int channels, QString path = "");
    ~WritableWaveFileModel();

    /**
     * Call addSamples to append a block of samples to the end of the
     * file.  Caller should also call setWriteProportion() to update
     * the progress of this file, if it has a known end point, and
     * should call writeComplete() when the file has been written.
     */
    virtual bool addSamples(const float *const *samples, sv_frame_t count);

    /**
     * Set the proportion of the file which has been written so far,
     * as a percentage. This may be used to indicate progress.
     *
     * Note that this differs from the "completion" percentage
     * reported through isReady()/getCompletion(). That percentage is
     * updated when "internal processing has advanced... but the model
     * has not changed externally", i.e. it reports progress in
     * calculating the initial state of a model. In contrast, an
     * update to setWriteProportion corresponds to a change in the
     * externally visible state of the model (i.e. it contains more
     * data than before).
     */
    void setWriteProportion(int proportion);

    /**
     * Indicate that writing is complete. You should call this even if
     * you have never called setWriteProportion().
     */
    void writeComplete();

    static const int PROPORTION_UNKNOWN;
    
    /**
     * Get the proportion of the file which has been written so far,
     * as a percentage. Return PROPORTION_UNKNOWN if unknown.
     */
    int getWriteProportion() const;
    
    bool isOK() const;
    bool isReady(int *) const;
    
    /**
     * Return the generation completion percentage of this model. This
     * is always 100, because the model is always in a complete state
     * -- it just contains varying amounts of data depending on how
     * much has been written.
     */
    virtual int getCompletion() const { return 100; }

    const ZoomConstraint *getZoomConstraint() const {
        static PowerOfSqrtTwoZoomConstraint zc;
        return &zc;
    }

    sv_frame_t getFrameCount() const;
    int getChannelCount() const { return m_channels; }
    sv_samplerate_t getSampleRate() const { return m_sampleRate; }
    sv_samplerate_t getNativeRate() const { return m_sampleRate; }

    QString getTitle() const {
        if (m_model) return m_model->getTitle();
        else return "";
    } 
    QString getMaker() const {
        if (m_model) return m_model->getMaker();
        else return "";
    }
    QString getLocation() const {
        if (m_model) return m_model->getLocation();
        else return "";
    }

    float getValueMinimum() const { return -1.0f; }
    float getValueMaximum() const { return  1.0f; }

    virtual sv_frame_t getStartFrame() const { return m_startFrame; }
    virtual sv_frame_t getEndFrame() const { return m_startFrame + getFrameCount(); }

    void setStartFrame(sv_frame_t startFrame);

    virtual std::vector<float> getData(int channel, sv_frame_t start, sv_frame_t count) const;

    virtual std::vector<std::vector<float>> getMultiChannelData(int fromchannel, int tochannel, sv_frame_t start, sv_frame_t count) const;

    virtual int getSummaryBlockSize(int desired) const;

    virtual void getSummaries(int channel, sv_frame_t start, sv_frame_t count,
                              RangeBlock &ranges, int &blockSize) const;

    virtual Range getSummary(int channel, sv_frame_t start, sv_frame_t count) const;

    QString getTypeName() const { return tr("Writable Wave File"); }

    virtual void toXml(QTextStream &out,
                       QString indent = "",
                       QString extraAttributes = "") const;

protected:
    ReadOnlyWaveFileModel *m_model;
    WavFileWriter *m_writer;
    WavFileReader *m_reader;
    sv_samplerate_t m_sampleRate;
    int m_channels;
    sv_frame_t m_frameCount;
    sv_frame_t m_startFrame;
    int m_proportion;
};

#endif

