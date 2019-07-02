/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
   
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "WaveformOversampler.h"

#include "base/Profiler.h"

#include "data/model/DenseTimeValueModel.h"

floatvec_t
WaveformOversampler::getOversampledData(const DenseTimeValueModel &source,
                                        int channel,
                                        sv_frame_t sourceStartFrame,
                                        sv_frame_t sourceFrameCount,
                                        int oversampleBy)
{
    Profiler profiler("WaveformOversampler::getOversampledData");
    
    // Oversampled at a fixed ratio of m_filterRatio
    floatvec_t fixedRatio = getFixedRatioData(source, channel,
                                              sourceStartFrame,
                                              sourceFrameCount);
    sv_frame_t fixedCount = fixedRatio.size();
    sv_frame_t targetCount = (fixedCount / m_filterRatio) * oversampleBy;

    // And apply linear interpolation to the desired factor
    
    floatvec_t result(targetCount, 0.f);

    for (int i = 0; i < targetCount; ++i) {
        double pos = (double(i) / oversampleBy) * m_filterRatio;
        double diff = pos - floor(pos);
        int ix = int(floor(pos));
        double interpolated = (1.0 - diff) * fixedRatio[ix];
        if (in_range_for(fixedRatio, ix + 1)) {
            interpolated += diff * fixedRatio[ix + 1];
        }
        result[i] = float(interpolated);
    }

    return result;
}

floatvec_t
WaveformOversampler::getFixedRatioData(const DenseTimeValueModel &source,
                                       int channel,
                                       sv_frame_t sourceStartFrame,
                                       sv_frame_t sourceFrameCount)
{
    Profiler profiler("WaveformOversampler::getFixedRatioData");
    
    sv_frame_t sourceLength = source.getEndFrame();
    
    if (sourceStartFrame + sourceFrameCount > sourceLength) {
        sourceFrameCount = sourceLength - sourceStartFrame;
        if (sourceFrameCount <= 0) return {};
    }

    sv_frame_t targetFrameCount = sourceFrameCount * m_filterRatio;
    
    sv_frame_t filterLength = m_filter.size(); // NB this is known to be odd
    sv_frame_t filterTailOut = (filterLength - 1) / 2;
    sv_frame_t filterTailIn = filterTailOut / m_filterRatio;

    floatvec_t oversampled(targetFrameCount, 0.f);

    sv_frame_t i0 = sourceStartFrame - filterTailIn;
    if (i0 < 0) {
        i0 = 0;
    }
    sv_frame_t i1 = sourceStartFrame + sourceFrameCount + filterTailIn;
    if (i1 > sourceLength) {
        i1 = sourceLength;
    }
    
    floatvec_t sourceData = source.getData(channel, i0, i1 - i0);
    
    for (sv_frame_t i = i0; i < i1; ++i) {
        float v = sourceData[i - i0];
        sv_frame_t outOffset =
            (i - sourceStartFrame) * m_filterRatio - filterTailOut;
        for (sv_frame_t j = 0; j < filterLength; ++j) {
            sv_frame_t outIndex = outOffset + j;
            if (outIndex < 0 || outIndex >= targetFrameCount) {
                continue;
            }
            oversampled[outIndex] += v * m_filter[j];
        }
    }

    return oversampled;
}

int
WaveformOversampler::m_filterRatio = 8;

/// Precalculated windowed sinc FIR filter for oversampling ratio of 8
floatvec_t
WaveformOversampler::m_filter {
    2.0171043153063023E-4, 2.887198196326776E-4,
    3.410439309101285E-4, 3.4267123819805857E-4,
    2.843462511901066E-4, 1.6636986363946504E-4,
    -4.5940658605786285E-18, -1.9299665002484582E-4,
    -3.8279951732549946E-4, -5.357990649609105E-4,
    -6.201170748425957E-4, -6.11531555444137E-4,
    -4.987822892899791E-4, -2.872272251922189E-4,
    -7.822991648518709E-19, 3.2382854144162815E-4,
    6.341027017666046E-4, 8.769331519465396E-4,
    0.001003535186382615, 9.791732608026272E-4,
    7.906678783612421E-4, 4.5101220009813206E-4,
    -3.039648514978356E-18, -4.996532051508215E-4,
    -9.704877518513666E-4, -0.0013318083550190923,
    -0.0015128911871247466, -0.0014658111457301016,
    -0.0011756800671747431, -6.663214707558645E-4,
    1.0713650598357415E-17, 7.292959363289514E-4,
    0.0014084768982220279, 0.0019222969998680237,
    0.0021721723797956524, 0.0020938999751673173,
    0.0016712330766289326, 9.427050283841188E-4,
    -5.656965938821965E-18, -0.0010225643654040554,
    -0.001966437013513757, -0.002672722670880038,
    -0.00300806671037164, -0.00288843624179131,
    -0.0022967244980623574, -0.0012908081494665458,
    -5.1499690577098E-18, 0.0013904094522721,
    0.0026648961861419334, 0.0036103002009868065,
    0.004050469159316014, 0.0038774554290217484,
    0.0030739396559265075, 0.001722603817632299,
    -9.130030250503607E-18, -0.0018451873718735516,
    -0.0035270571169279162, -0.004765847116110058,
    -0.0053332982334767624, -0.005092831604550132,
    -0.00402770012894082, -0.0022517645624319594,
    3.2752446397299053E-17, 0.0024010765839506923,
    0.004579613038976446, 0.006174912111845945,
    0.00689578873526276, 0.006571541332393174,
    0.005186887306036285, 0.002894248521447605,
    -1.336645565990815E-17, -0.0030747336558684963,
    -0.0058540294958507235, -0.007879546416595632,
    -0.008784519668338507, -0.008357645279493864,
    -0.006586046547485615, -0.003669217725935383,
    -1.9348975378752276E-17, 0.0038863208094135626,
    0.007388553022623823, 0.009931080628244226,
    0.011056594746806033, 0.010505398026651453,
    0.008267906564613223, 0.00460048159140493,
    -1.816145184081109E-17, -0.004861124757802925,
    -0.009231379891669668, -0.012394511669674028,
    -0.01378467517229709, -0.013084177592430083,
    -0.010287380585427207, -0.00571879407588959,
    7.520535431851951E-17, 0.006032144534828161,
    0.011445734103106982, 0.015355551390625357,
    0.017065088242935025, 0.016186450815185452,
    0.012718051439340603, 0.0070655888687995785,
    -2.3209664144996714E-17, -0.007444328311482942,
    -0.01411821163125819, -0.018932253281654043,
    -0.02103125585328301, -0.019941019333653123,
    -0.015663002335303704, -0.008699245445932525,
    2.5712475624993567E-17, 0.009161748270723635,
    0.0173729814451255, 0.023294901939595228,
    0.02587678878709242, 0.02453592568963366,
    0.01927365323131565, 0.010706050935569809,
    -2.8133472199037193E-17, -0.011280308241551094,
    -0.02139710071477064, -0.02870170641615764,
    -0.031897218249350504, -0.030260140480986304,
    -0.023784294156618507, -0.013220449772289724,
    3.042099156841831E-17, 0.013951594368737923,
    0.0264884258371512, 0.03556693609945249,
    0.03957036852169639, 0.0375845888664677,
    0.029579845398822833, 0.016465167405787955,
    -3.2524514488155654E-17, -0.017431115375410273,
    -0.03315356952091943, -0.04460179422099746,
    -0.0497244634100025, -0.04733366619358394,
    -0.037341081614037944, -0.020838316594689998,
    3.439626695384943E-17, 0.022185914535618936,
    0.04232958202159685, 0.05713801867687856,
    0.06393033000280622, 0.06109191933191721,
    0.04839482380906132, 0.027127167584840003,
    -3.5992766927138734E-17, -0.029168755716052385,
    -0.055960213335110184, -0.07598693477350407,
    -0.08556575102599769, -0.08233350786406181,
    -0.06571046000158454, -0.03713224848702707,
    3.727625511036616E-17, 0.04066438975848791,
    0.07882920057770397, 0.10826166115536123,
    0.12343378955977465, 0.12040455825217859,
    0.09755344650130694, 0.056053367635801106,
    -3.8215953158245473E-17, -0.0638435745677513,
    -0.12667849902789644, -0.17861887575594584,
    -0.20985333136704623, -0.21188193950868073,
    -0.17867464086818077, -0.10760048593620072,
    3.8789099095340224E-17, 0.13868670259490817,
    0.29927055936918734, 0.46961864377510765,
    0.6358321371992203, 0.7836674214332147,
    0.9000377382311825, 0.9744199685311685,
    1.0000000000000004, 0.9744199685311685,
    0.9000377382311825, 0.7836674214332147,
    0.6358321371992203, 0.46961864377510765,
    0.29927055936918734, 0.13868670259490817,
    3.8789099095340224E-17, -0.10760048593620072,
    -0.17867464086818077, -0.21188193950868073,
    -0.20985333136704623, -0.17861887575594584,
    -0.12667849902789644, -0.0638435745677513,
    -3.8215953158245473E-17, 0.056053367635801106,
    0.09755344650130694, 0.12040455825217859,
    0.12343378955977465, 0.10826166115536123,
    0.07882920057770397, 0.04066438975848791,
    3.727625511036616E-17, -0.03713224848702707,
    -0.06571046000158454, -0.08233350786406181,
    -0.08556575102599769, -0.07598693477350407,
    -0.055960213335110184, -0.029168755716052385,
    -3.5992766927138734E-17, 0.027127167584840003,
    0.04839482380906132, 0.06109191933191721,
    0.06393033000280622, 0.05713801867687856,
    0.04232958202159685, 0.022185914535618936,
    3.439626695384943E-17, -0.020838316594689998,
    -0.037341081614037944, -0.04733366619358394,
    -0.0497244634100025, -0.04460179422099746,
    -0.03315356952091943, -0.017431115375410273,
    -3.2524514488155654E-17, 0.016465167405787955,
    0.029579845398822833, 0.0375845888664677,
    0.03957036852169639, 0.03556693609945249,
    0.0264884258371512, 0.013951594368737923,
    3.042099156841831E-17, -0.013220449772289724,
    -0.023784294156618507, -0.030260140480986304,
    -0.031897218249350504, -0.02870170641615764,
    -0.02139710071477064, -0.011280308241551094,
    -2.8133472199037193E-17, 0.010706050935569809,
    0.01927365323131565, 0.02453592568963366,
    0.02587678878709242, 0.023294901939595228,
    0.0173729814451255, 0.009161748270723635,
    2.5712475624993567E-17, -0.008699245445932525,
    -0.015663002335303704, -0.019941019333653123,
    -0.02103125585328301, -0.018932253281654043,
    -0.01411821163125819, -0.007444328311482942,
    -2.3209664144996714E-17, 0.0070655888687995785,
    0.012718051439340603, 0.016186450815185452,
    0.017065088242935025, 0.015355551390625357,
    0.011445734103106982, 0.006032144534828161,
    7.520535431851951E-17, -0.00571879407588959,
    -0.010287380585427207, -0.013084177592430083,
    -0.01378467517229709, -0.012394511669674028,
    -0.009231379891669668, -0.004861124757802925,
    -1.816145184081109E-17, 0.00460048159140493,
    0.008267906564613223, 0.010505398026651453,
    0.011056594746806033, 0.009931080628244226,
    0.007388553022623823, 0.0038863208094135626,
    -1.9348975378752276E-17, -0.003669217725935383,
    -0.006586046547485615, -0.008357645279493864,
    -0.008784519668338507, -0.007879546416595632,
    -0.0058540294958507235, -0.0030747336558684963,
    -1.336645565990815E-17, 0.002894248521447605,
    0.005186887306036285, 0.006571541332393174,
    0.00689578873526276, 0.006174912111845945,
    0.004579613038976446, 0.0024010765839506923,
    3.2752446397299053E-17, -0.0022517645624319594,
    -0.00402770012894082, -0.005092831604550132,
    -0.0053332982334767624, -0.004765847116110058,
    -0.0035270571169279162, -0.0018451873718735516,
    -9.130030250503607E-18, 0.001722603817632299,
    0.0030739396559265075, 0.0038774554290217484,
    0.004050469159316014, 0.0036103002009868065,
    0.0026648961861419334, 0.0013904094522721,
    -5.1499690577098E-18, -0.0012908081494665458,
    -0.0022967244980623574, -0.00288843624179131,
    -0.00300806671037164, -0.002672722670880038,
    -0.001966437013513757, -0.0010225643654040554,
    -5.656965938821965E-18, 9.427050283841188E-4,
    0.0016712330766289326, 0.0020938999751673173,
    0.0021721723797956524, 0.0019222969998680237,
    0.0014084768982220279, 7.292959363289514E-4,
    1.0713650598357415E-17, -6.663214707558645E-4,
    -0.0011756800671747431, -0.0014658111457301016,
    -0.0015128911871247466, -0.0013318083550190923,
    -9.704877518513666E-4, -4.996532051508215E-4,
    -3.039648514978356E-18, 4.5101220009813206E-4,
    7.906678783612421E-4, 9.791732608026272E-4,
    0.001003535186382615, 8.769331519465396E-4,
    6.341027017666046E-4, 3.2382854144162815E-4,
    -7.822991648518709E-19, -2.872272251922189E-4,
    -4.987822892899791E-4, -6.11531555444137E-4,
    -6.201170748425957E-4, -5.357990649609105E-4,
    -3.8279951732549946E-4, -1.9299665002484582E-4,
    -4.5940658605786285E-18, 1.6636986363946504E-4,
    2.843462511901066E-4, 3.4267123819805857E-4,
    3.410439309101285E-4, 2.887198196326776E-4,
    2.0171043153063023E-4
};
    
