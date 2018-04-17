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

#include "CSVFileWriter.h"
#include "CSVStreamWriter.h"

#include "model/Model.h"
#include "model/SparseOneDimensionalModel.h"
#include "model/SparseTimeValueModel.h"
#include "model/NoteModel.h"
#include "model/TextModel.h"

#include "base/TempWriteFile.h"
#include "base/Exceptions.h"
#include "base/Selection.h"

#include <QFile>
#include <QTextStream>
#include <exception>
#include <numeric>

CSVFileWriter::CSVFileWriter(QString path,
                             Model *model,
                             QString delimiter,
                             DataExportOptions options) :
    m_path(path),
    m_model(model),
    m_error(""),
    m_delimiter(delimiter),
    m_options(options)
{
}

CSVFileWriter::~CSVFileWriter()
{
}

bool
CSVFileWriter::isOK() const
{
    return m_error == "";
}

QString
CSVFileWriter::getError() const
{
    return m_error;
}

void
CSVFileWriter::write()
{
    Selection all {
        m_model->getStartFrame(),
        m_model->getEndFrame()
    };
    MultiSelection selections;
    selections.addSelection(all);
    writeSelection(&selections); 
}

void
CSVFileWriter::writeSelection(MultiSelection *selection)
{
    try {
        TempWriteFile temp(m_path);

        QFile file(temp.getTemporaryFilename());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_error = tr("Failed to open file %1 for writing")
                .arg(temp.getTemporaryFilename());
            return;
        }
    
        QTextStream out(&file);

        bool completed = false;

        const auto nFramesToWrite = std::accumulate(
            selection->getSelections().begin(),
            selection->getSelections().end(),
            0,
            [](sv_frame_t acc, const Selection& current) -> sv_frame_t {
                return acc + (current.getEndFrame() - current.getStartFrame());
            }
        );

        sv_frame_t nFramesWritten = 0;
        const auto createProgressCalculator = [
            &nFramesWritten,
            &nFramesToWrite
        ](sv_frame_t nFramesToWriteForSelection) {
            const auto nFramesWrittenAtSelectionStart = nFramesWritten;
            nFramesWritten += nFramesToWriteForSelection;
            return [
                &nFramesWritten,
                &nFramesToWrite,
                nFramesWrittenAtSelectionStart
            ] (sv_frame_t nFramesWrittenForSelection) {
                const auto nFramesWrittenSoFar = (
                    nFramesWrittenAtSelectionStart + nFramesWrittenForSelection
                );
                return 100 * nFramesWrittenSoFar / nFramesToWrite;
            };
        };

        for (const auto& bounds : selection->getSelections()) {
            completed = CSV::writeToStreamInChunks(
                out,
                *m_model,
                bounds,
                m_reporter,
                m_delimiter,
                m_options,
                16384,
                createProgressCalculator
            );
            if (!completed) {
                break;
            } 
        }

        file.close();
        if (completed) {
            temp.moveToTarget();
        }

    } catch (FileOperationFailed &f) {
        m_error = f.what();
    } catch (const std::exception &e) { // ProgressReporter could throw
        m_error = e.what();
    }
}
