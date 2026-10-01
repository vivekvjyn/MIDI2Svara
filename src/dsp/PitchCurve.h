#pragma once
#include "NoteData.h"

class PitchCurveInterpolator
{
public:
    
    static float interpolate(const std::vector<PitchPoint>& points, double time, bool includeVibrato = true);

    
    static std::vector<PitchPoint> generateVibrato(
        double startTime, double duration,
        float rate, float depth,
        int shape,      
        int direction,   
        float fadeIn, float fadeOut);

    
    static std::vector<PitchPoint> generateGamaka(
        const String& patternName,
        double startTime, double duration, float intensity);

    
    static void smooth(std::vector<PitchPoint>& points, float amount);

    
    
    static void simplify(std::vector<PitchPoint>& points, float tolerance = 0.05f);

    
    static void smoothRange(std::vector<PitchPoint>& points, int startIdx, int endIdx, float amount);

    
    static void simplifyRange(std::vector<PitchPoint>& points, int startIdx, int endIdx, float tolerance = 0.05f);

    
    static void mergeIntoCurve(std::vector<PitchPoint>& curve,
                               const std::vector<PitchPoint>& overlay,
                               double offsetTime);

private:
    static float catmullRom(float p0, float p1, float p2, float p3, float t);
};
