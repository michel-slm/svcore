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

#include "AudioFileReaderFactory.h"

#include "WavFileReader.h"
#include "DecodingWavFileReader.h"
#include "OggVorbisFileReader.h"
#include "MP3FileReader.h"
#include "CoreAudioFileReader.h"
#include "AudioFileSizeEstimator.h"

#include "base/StorageAdviser.h"

#include <QString>
#include <QFileInfo>
#include <iostream>

QString
AudioFileReaderFactory::getKnownExtensions()
{
    std::set<QString> extensions;

    WavFileReader::getSupportedExtensions(extensions);
#ifdef HAVE_MAD
    MP3FileReader::getSupportedExtensions(extensions);
#endif
#ifdef HAVE_OGGZ
#ifdef HAVE_FISHSOUND
    OggVorbisFileReader::getSupportedExtensions(extensions);
#endif
#endif
#ifdef HAVE_COREAUDIO
    CoreAudioFileReader::getSupportedExtensions(extensions);
#endif

    QString rv;
    for (std::set<QString>::const_iterator i = extensions.begin();
         i != extensions.end(); ++i) {
        if (i != extensions.begin()) rv += " ";
        rv += "*." + *i;
    }

    return rv;
}

AudioFileReader *
AudioFileReaderFactory::createReader(FileSource source,
                                     Parameters params,
                                     ProgressReporter *reporter)
{
    QString err;

    SVDEBUG << "AudioFileReaderFactory::createReader(\"" << source.getLocation() << "\"): Requested rate: " << params.targetRate << (params.targetRate == 0 ? " (use source rate)" : "") << endl;

    if (!source.isOK()) {
        SVDEBUG << "AudioFileReaderFactory::createReader(\"" << source.getLocation() << "\": Failed to retrieve source (transmission error?): " << source.getErrorString() << endl;
        return 0;
    }

    if (!source.isAvailable()) {
        SVDEBUG << "AudioFileReaderFactory::createReader(\"" << source.getLocation() << "\": Source not found" << endl;
        return 0;
    }

    AudioFileReader *reader = 0;

    sv_samplerate_t targetRate = params.targetRate;
    bool normalised = (params.normalisation == Normalisation::Peak);
  
    sv_frame_t estimatedSamples = 
        AudioFileSizeEstimator::estimate(source, targetRate);
    
    CodedAudioFileReader::CacheMode cacheMode =
        CodedAudioFileReader::CacheInTemporaryFile;

    if (estimatedSamples > 0) {
        size_t kb = (estimatedSamples * sizeof(float)) / 1024;
        StorageAdviser::Recommendation rec =
            StorageAdviser::recommend(StorageAdviser::SpeedCritical, kb, kb);
        if ((rec & StorageAdviser::UseMemory) ||
            (rec & StorageAdviser::PreferMemory)) {
            cacheMode = CodedAudioFileReader::CacheInMemory;
        }
    }
    
    CodedAudioFileReader::DecodeMode decodeMode =
        (params.threadingMode == ThreadingMode::Threaded ?
         CodedAudioFileReader::DecodeThreaded :
         CodedAudioFileReader::DecodeAtOnce);

    // We go through the set of supported readers at most twice: once
    // picking out only the readers that claim to support the given
    // file's extension or MIME type, and (if that fails) again
    // providing the file to every reader in turn regardless of
    // extension or type. (If none of the readers claim to support a
    // file, that may just mean its extension is missing or
    // misleading. We have to be confident that the reader won't open
    // just any old text file or whatever and pretend it's succeeded.)

    for (int any = 0; any <= 1; ++any) {

        bool anyReader = (any > 0);

        if (anyReader || WavFileReader::supports(source)) {

            reader = new WavFileReader(source);

            sv_samplerate_t fileRate = reader->getSampleRate();

            if (reader->isOK() &&
                (!reader->isQuicklySeekable() ||
                 normalised ||
                 (cacheMode == CodedAudioFileReader::CacheInMemory) ||
                 (targetRate != 0 && fileRate != targetRate))) {

                SVDEBUG << "AudioFileReaderFactory::createReader: WAV file rate: " << reader->getSampleRate() << ", normalised " << normalised << ", seekable " << reader->isQuicklySeekable() << ", in memory " << (cacheMode == CodedAudioFileReader::CacheInMemory) << ", creating decoding reader" << endl;
            
                delete reader;
                reader = new DecodingWavFileReader
                    (source,
                     decodeMode, cacheMode,
                     targetRate ? targetRate : fileRate,
                     normalised,
                     reporter);
            }

            if (reader->isOK()) {
                return reader;
            } else {
                delete reader;
            }
        }
    
#ifdef HAVE_OGGZ
#ifdef HAVE_FISHSOUND
        if (anyReader || OggVorbisFileReader::supports(source)) {

            reader = new OggVorbisFileReader
                (source, decodeMode, cacheMode, targetRate, normalised, reporter);

            if (reader->isOK()) {
                return reader;
            } else {
                delete reader;
            }
        }
#endif
#endif

#ifdef HAVE_MAD
        if (anyReader || MP3FileReader::supports(source)) {

            MP3FileReader::GaplessMode gapless =
                params.gaplessMode == GaplessMode::Gapless ?
                MP3FileReader::GaplessMode::Gapless :
                MP3FileReader::GaplessMode::Gappy;
            
            reader = new MP3FileReader
                (source, decodeMode, cacheMode, gapless,
                 targetRate, normalised, reporter);

            if (reader->isOK()) {
                return reader;
            } else {
                delete reader;
            }
        }
#endif

#ifdef HAVE_COREAUDIO
        if (anyReader || CoreAudioFileReader::supports(source)) {

            reader = new CoreAudioFileReader
                (source, decodeMode, cacheMode, targetRate, normalised, reporter);

            if (reader->isOK()) {
                return reader;
            } else {
                delete reader;
            }
        }
#endif

    }
    
    SVDEBUG << "AudioFileReaderFactory::Failed to create a reader for "
            << "url \"" << source.getLocation()
            << "\" (content type \""
            << source.getContentType() << "\")" << endl;
    return nullptr;
}

