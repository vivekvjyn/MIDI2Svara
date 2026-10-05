#include "PitchCurve.h"
#include <cmath>
#include <algorithm>

float PitchCurveInterpolator::catmullRom(float p0, float p1, float p2, float p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
                    (-p0 + p2) * t +
                    (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                    (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

float PitchCurveInterpolator::interpolate(const std::vector<PitchPoint>& points, double time, bool includeVibrato)
{
    if (points.empty())
        return 0.0f;

    if (points.size() == 1)
        return (float)points[0].pitchOffset;

    if (time <= points.front().time)
        return (float)points.front().pitchOffset;
    if (time >= points.back().time)
        return (float)points.back().pitchOffset;

    size_t i = 0;
    for (i = 0; i < points.size() - 1; ++i)
    {
        if (time >= points[i].time && time < points[i + 1].time)
            break;
    }

    double segStart = points[i].time;
    double segEnd = points[i + 1].time;
    float t = (segEnd > segStart) ? (float)((time - segStart) / (segEnd - segStart)) : 0.0f;

    float result;

    if (points[i].curveType == PitchPoint::CurveType::Step)
    {
        result = (float)points[i].pitchOffset;
    }
    else if (points[i].curveType == PitchPoint::CurveType::Linear)
    {
        result = (float)(points[i].pitchOffset + (points[i + 1].pitchOffset - points[i].pitchOffset) * t);
    }
    else
    {
        float p1 = (float)points[i].pitchOffset;
        float p2 = (float)points[i + 1].pitchOffset;
        float p0 = (i > 0) ? (float)points[i - 1].pitchOffset : p1;
        float p3 = (i + 2 < points.size()) ? (float)points[i + 2].pitchOffset : p2;

        float biasVal = points[i].bias;
        if (std::abs(biasVal) > 0.001f)
        {
            float warp = std::sin(MathConstants<float>::pi * t);
            t = jlimit(0.0f, 1.0f, t + biasVal * warp * 0.3f);
        }

        float c1 = points[i].curvature;
        float c2 = points[i + 1].curvature;
        float m1 = c1 * (p2 - p0) * 0.5f;
        float m2 = c2 * (p3 - p1) * 0.5f;

        float t2 = t * t;
        float t3 = t2 * t;
        float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        float h10 = t3 - 2.0f * t2 + t;
        float h01 = -2.0f * t3 + 3.0f * t2;
        float h11 = t3 - t2;

        float hermite = h00 * p1 + h10 * m1 + h01 * p2 + h11 * m2;

        float pitchDiff = std::abs(p1 - p2);
        float straightenFactor = jlimit(0.0f, 1.0f, pitchDiff / 0.5f);

        float linear = p1 + (p2 - p1) * t;
        result = linear + (hermite - linear) * straightenFactor;
    }

    if (includeVibrato && points[i].vibrato.enabled)
    {
        auto& vib = points[i].vibrato;
        float segDur = (float)(segEnd - segStart);
        float localTime = (float)(time - segStart);

        float envelope = 1.0f;
        float fadeInTime = vib.fadeInFrac * segDur;
        float fadeOutTime = vib.fadeOutFrac * segDur;
        if (fadeInTime > 0.001f && localTime < fadeInTime)
            envelope *= localTime / fadeInTime;
        float timeFromEnd = segDur - localTime;
        if (fadeOutTime > 0.001f && timeFromEnd < fadeOutTime)
            envelope *= timeFromEnd / fadeOutTime;

        float phase = MathConstants<float>::twoPi * vib.rate * localTime;
        float wave = 0.0f;
        switch (vib.waveform)
        {
            case VibratoWaveform::Sine:
                wave = std::sin(phase);
                break;
            case VibratoWaveform::Triangle:
            {
                float p = phase / MathConstants<float>::twoPi;
                wave = 4.0f * std::abs(p - std::floor(p + 0.5f)) - 1.0f;
                break;
            }
            case VibratoWaveform::Humanized:
            {
                
                float n1 = std::sin(phase * 7.31f) * 0.15f;
                float n2 = std::sin(phase * 3.73f) * 0.1f;
                wave = std::sin(phase + n1) * (1.0f + n2);
                break;
            }
            case VibratoWaveform::Vocal:
            {
                
                wave = std::sin(phase) + 0.22f * std::sin(phase * 2.0f + 0.5f);
                wave *= 0.82f;
                break;
            }
            case VibratoWaveform::Violin:
            {
                
                wave = std::sin(phase) + 0.15f * std::sin(phase * 2.02f)
                     + 0.08f * std::sin(phase * 3.01f);
                wave *= 0.8f;
                break;
            }
            case VibratoWaveform::Sitar:
            {
                
                wave = std::sin(phase) * (1.0f + 0.35f * std::sin(phase * 0.333f + 0.7f));
                wave *= 0.74f;
                break;
            }
            case VibratoWaveform::NumTypes:
                break;
        }
        float vibOsc = wave * vib.depth;

        result += (vibOsc + vib.offset) * envelope;
    }

    return result;
}

std::vector<PitchPoint> PitchCurveInterpolator::generateVibrato(
    double startTime, double duration,
    float rate, float depth,
    int shape, int direction,
    float fadeIn, float fadeOut)
{
    std::vector<PitchPoint> points;
    const double step = 0.01;

    for (double t = 0.0; t <= duration; t += step)
    {
        double absTime = startTime + t;
        float phase = (float)(t * rate * MathConstants<double>::twoPi);
        float value = 0.0f;

        switch (shape)
        {
            case 0:
                value = std::sin(phase);
                break;
            case 1:
                value = (float)(2.0 * std::asin(std::sin(phase)) / MathConstants<double>::pi);
                break;
            case 2:
                value = (float)(2.0 * (t * rate - std::floor(t * rate + 0.5)));
                break;
            case 3:
                value = std::sin(phase) >= 0.0f ? 1.0f : -1.0f;
                break;
        }

        switch (direction)
        {
            case 1: value = std::abs(value); break;
            case 2: value = -std::abs(value); break;
            case 3: value = value > 0 ? value * 0.7f : value * 1.3f; break;
            default: break;
        }

        float envelope = 1.0f;
        if (fadeIn > 0.0f && t < fadeIn)
            envelope *= (float)(t / fadeIn);
        if (fadeOut > 0.0f && (duration - t) < fadeOut)
            envelope *= (float)((duration - t) / fadeOut);

        float offset = value * depth * envelope / 100.0f;

        PitchPoint pp;
        pp.time = absTime;
        pp.pitchOffset = offset;
        pp.curveType = PitchPoint::CurveType::Smooth;
        points.push_back(pp);
    }

    return points;
}

std::vector<PitchPoint> PitchCurveInterpolator::generateGamaka(
    const String& patternName,
    double startTime, double duration, float intensity)
{
    std::vector<PitchPoint> points;

    struct PatternDef {
        float time;
        float pitch;
        float bias = 0.0f;
        float curvature = 1.0f;
    };

    std::vector<PatternDef> pattern;

    if (patternName == "kampita")
    {
        pattern = {{0.0f, 0.0f, 0.0f, 0.6f}, {0.15f, 1.0f}, {0.35f, -0.4f},
                   {0.55f, 0.8f}, {0.78f, -0.2f}, {1.0f, 0.0f, 0.0f, 0.5f}};
    }
    else if (patternName == "jaru_up")
    {
        pattern = {{0.0f, -2.0f, 0.8f}, {0.5f, -0.3f, -0.5f}, {1.0f, 0.0f, 0.0f, 0.4f}};
    }
    else if (patternName == "jaru_down")
    {
        pattern = {{0.0f, 0.0f, 0.5f, 0.4f}, {0.5f, -0.3f, -0.8f}, {1.0f, -2.0f}};
    }
    else if (patternName == "odukkal")
    {
        pattern = {{0.0f, 0.0f, 0.3f, 0.5f}, {0.3f, -0.5f, -0.6f}, {1.0f, -2.0f}};
    }
    else if (patternName == "orikkai")
    {
        pattern = {{0.0f, 0.0f}, {0.1f, 1.0f}, {0.22f, -0.4f},
                   {0.35f, 0.0f, 0.0f, 0.3f}, {1.0f, 0.0f}};
    }
    else if (patternName == "pratyahata")
    {
        pattern = {{0.0f, 0.0f, 0.4f, 0.8f}, {0.25f, 1.2f, -0.3f},
                   {0.6f, 0.1f, -0.4f, 0.5f}, {1.0f, 0.0f}};
    }
    else if (patternName == "ravai")
    {
        pattern = {{0.0f, 0.0f, 0.6f, 0.3f}, {0.12f, 1.5f, -0.5f},
                   {0.45f, 0.0f}, {1.0f, 0.0f}};
    }
    else if (patternName == "sphurita")
    {
        pattern = {{0.0f, 1.0f}, {0.15f, 0.0f}, {0.3f, 0.85f},
                   {0.5f, 0.0f}, {0.7f, 0.5f}, {1.0f, 0.0f}};
    }

    else if (patternName == "meend_up")
    {
        pattern = {{0.0f, -2.5f, 0.9f}, {1.0f, 0.0f, 0.0f, 0.4f}};
    }
    else if (patternName == "meend_down")
    {
        pattern = {{0.0f, 0.0f, -0.9f, 0.4f}, {1.0f, -2.5f}};
    }
    else if (patternName == "andolan")
    {
        pattern = {{0.0f, 0.0f}, {0.25f, 0.4f}, {0.5f, 0.0f},
                   {0.75f, -0.35f}, {1.0f, 0.0f}};
    }
    else if (patternName == "gamak")
    {
        pattern = {{0.0f, 0.0f}, {0.1f, 1.2f}, {0.25f, -0.8f}, {0.4f, 1.0f},
                   {0.55f, -0.7f}, {0.72f, 0.8f}, {0.88f, -0.3f}, {1.0f, 0.0f}};
    }
    else if (patternName == "krintan")
    {
        pattern = {{0.0f, 0.0f, 0.5f}, {0.08f, -1.0f, -0.5f}, {0.18f, 0.0f, 0.4f},
                   {0.28f, -0.8f, -0.4f}, {0.4f, 0.0f}, {1.0f, 0.0f}};
    }
    else if (patternName == "murki")
    {
        pattern = {{0.0f, 0.0f}, {0.06f, 0.5f}, {0.12f, -0.3f},
                   {0.2f, 0.4f}, {0.28f, 0.0f}, {1.0f, 0.0f}};
    }
    else if (patternName == "zamzama")
    {
        pattern = {{0.0f, 0.0f}, {0.1f, 1.0f}, {0.22f, -0.5f}, {0.34f, 0.7f},
                   {0.48f, -0.3f}, {0.65f, 0.3f, -0.3f, 0.5f}, {1.0f, 0.0f}};
    }

    else if (patternName == "trill")
    {
        pattern = {{0.0f, 0.0f}, {0.1f, 1.0f}, {0.2f, 0.0f}, {0.3f, 1.0f},
                   {0.4f, 0.0f}, {0.5f, 1.0f}, {0.6f, 0.0f}, {0.72f, 0.9f},
                   {0.85f, 0.0f}, {1.0f, 0.0f}};
    }
    else if (patternName == "qtr_shake")
    {
        pattern = {{0.0f, 0.0f}, {0.15f, 0.5f}, {0.3f, 0.0f}, {0.45f, 0.5f},
                   {0.6f, 0.0f}, {0.78f, 0.4f}, {0.92f, 0.0f}, {1.0f, 0.0f}};
    }
    else if (patternName == "maqam_slide")
    {
        pattern = {{0.0f, 0.0f, 0.6f}, {0.4f, 1.5f, 0.0f, 1.2f},
                   {0.7f, 1.5f, -0.6f}, {1.0f, 0.0f}};
    }
    else if (patternName == "ornament_turn")
    {
        pattern = {{0.0f, 0.0f}, {0.15f, 0.8f}, {0.35f, 0.0f},
                   {0.5f, -0.8f}, {0.7f, 0.0f}, {1.0f, 0.0f}};
    }

    else if (patternName == "bend_up")
    {
        pattern = {{0.0f, 0.0f, 0.9f}, {0.6f, 1.2f, -0.5f, 0.8f}, {1.0f, 2.0f, 0.0f, 0.3f}};
    }
    else if (patternName == "bend_down")
    {
        pattern = {{0.0f, 2.0f, -0.9f, 0.3f}, {0.4f, 0.8f, 0.5f, 0.8f}, {1.0f, 0.0f}};
    }
    else if (patternName == "scoop")
    {
        pattern = {{0.0f, -1.5f, -0.7f}, {0.35f, -0.1f, -0.4f, 0.5f}, {1.0f, 0.0f}};
    }
    else if (patternName == "fall_off")
    {
        pattern = {{0.0f, 0.0f}, {0.6f, 0.0f, 0.8f}, {1.0f, -2.5f}};
    }
    else if (patternName == "ghost_bend")
    {
        pattern = {{0.0f, 0.0f, 0.5f}, {0.35f, 1.0f, 0.0f, 1.2f},
                   {0.65f, 1.0f, -0.5f}, {1.0f, 0.0f}};
    }
    else if (patternName == "blues_curl")
    {
        pattern = {{0.0f, -0.3f, 0.5f}, {0.12f, 0.5f, -0.3f},
                   {0.4f, 0.0f}, {0.65f, -0.1f, 0.3f, 0.5f}, {1.0f, 0.0f}};
    }

    else if (patternName == "nori")
    {
        pattern = {{0.0f, 0.0f}, {0.12f, 0.3f}, {0.28f, 0.0f},
                   {0.45f, 0.2f}, {0.65f, 0.0f}, {1.0f, 0.0f}};
    }
    else if (patternName == "yuri")
    {
        pattern = {{0.0f, 0.0f}, {0.25f, 0.25f}, {0.5f, -0.15f},
                   {0.75f, 0.15f}, {1.0f, 0.0f}};
    }
    else if (patternName == "oshide")
    {
        pattern = {{0.0f, 0.0f, 0.7f, 0.3f}, {0.15f, 1.0f, 0.0f, 1.3f},
                   {0.4f, 1.0f, -0.7f}, {1.0f, 0.0f}};
    }

    else if (patternName == "cry")
    {
        pattern = {{0.0f, -0.5f, 0.7f, 0.3f}, {0.08f, 1.8f, -0.6f},
                   {0.4f, 0.2f, -0.4f, 0.5f}, {1.0f, 0.0f}};
    }
    else if (patternName == "ayeo")
    {
        pattern = {{0.0f, 0.0f}, {0.12f, 0.8f}, {0.25f, -0.3f}, {0.38f, 1.2f},
                   {0.55f, 0.0f}, {0.72f, 0.5f}, {1.0f, 0.0f}};
    }
    else if (patternName == "quejio")
    {
        pattern = {{0.0f, 0.0f, 0.5f}, {0.07f, -1.0f, 0.6f, 0.4f}, {0.15f, 1.5f, -0.3f},
                   {0.35f, 1.0f}, {0.55f, 0.5f}, {0.78f, 0.15f, -0.3f, 0.5f}, {1.0f, 0.0f}};
    }

    else if (patternName == "vocal_scoop")
    {
        pattern = {{0.0f, -0.8f, -0.6f}, {0.3f, -0.05f, -0.3f, 0.4f}, {1.0f, 0.0f}};
    }
    else if (patternName == "vocal_drop")
    {
        pattern = {{0.0f, 0.0f}, {0.55f, 0.0f, 0.8f}, {1.0f, -1.5f}};
    }
    else if (patternName == "vocal_riff")
    {
        pattern = {{0.0f, 0.0f}, {0.1f, 0.6f}, {0.2f, -0.3f}, {0.32f, 0.8f},
                   {0.45f, -0.5f}, {0.58f, 0.3f}, {1.0f, 0.0f}};
    }
    else if (patternName == "portamento_up")
    {
        pattern = {{0.0f, -2.0f, 0.6f}, {1.0f, 0.0f}};
    }
    else if (patternName == "portamento_dn")
    {
        pattern = {{0.0f, 0.0f, -0.6f}, {1.0f, -2.0f}};
    }

    else
    {
        pattern = {{0.0f, 0.0f}, {0.25f, 0.5f}, {0.5f, 0.0f}, {0.75f, -0.5f}, {1.0f, 0.0f}};
    }

    for (auto& p : pattern)
    {
        PitchPoint pp;
        pp.time = startTime + p.time * duration;
        pp.pitchOffset = p.pitch * intensity;
        pp.curveType = PitchPoint::CurveType::Smooth;
        pp.bias = p.bias;
        pp.curvature = p.curvature;
        points.push_back(pp);
    }

    return points;
}

void PitchCurveInterpolator::smooth(std::vector<PitchPoint>& points, float amount)
{
    if (points.size() < 3) return;
    smoothRange(points, 0, (int)points.size() - 1, amount);
}

void PitchCurveInterpolator::smoothRange(std::vector<PitchPoint>& points, int startIdx, int endIdx, float amount)
{
    if (points.size() < 3) return;
    startIdx = jmax(0, startIdx);
    endIdx = jmin((int)points.size() - 1, endIdx);
    if (endIdx - startIdx < 2) return;

    int passes = 3;
    for (int pass = 0; pass < passes; ++pass)
    {
        float passAmount = amount * (1.0f - (float)pass * 0.25f);

        std::vector<double> smoothed(points.size());
        for (size_t i = 0; i < points.size(); ++i)
            smoothed[i] = points[i].pitchOffset;

        for (int i = startIdx + 1; i < endIdx; ++i)
        {
            
            double sum = 0.0;
            double weight = 0.0;

            if (i - 2 >= startIdx) { sum += points[(size_t)(i - 2)].pitchOffset * 1.0; weight += 1.0; }
            if (i - 1 >= startIdx) { sum += points[(size_t)(i - 1)].pitchOffset * 4.0; weight += 4.0; }
            sum += points[(size_t)i].pitchOffset * 6.0; weight += 6.0;
            if (i + 1 <= endIdx) { sum += points[(size_t)(i + 1)].pitchOffset * 4.0; weight += 4.0; }
            if (i + 2 <= endIdx) { sum += points[(size_t)(i + 2)].pitchOffset * 1.0; weight += 1.0; }

            double filtered = sum / weight;
            smoothed[(size_t)i] = points[(size_t)i].pitchOffset + (filtered - points[(size_t)i].pitchOffset) * passAmount;
        }

        for (int i = startIdx + 1; i < endIdx; ++i)
        {
            points[(size_t)i].pitchOffset = smoothed[(size_t)i];

            double timeMid = (points[(size_t)(i - 1)].time + points[(size_t)(i + 1)].time) * 0.5;
            points[(size_t)i].time += (timeMid - points[(size_t)i].time) * passAmount * 0.15;
        }
    }
}

static void rdpSimplify(const std::vector<PitchPoint>& input, int startIdx, int endIdx,
                         float tolerance, std::vector<bool>& keep)
{
    if (endIdx <= startIdx + 1) return;

    double t0 = input[(size_t)startIdx].time;
    double p0 = input[(size_t)startIdx].pitchOffset;
    double t1 = input[(size_t)endIdx].time;
    double p1 = input[(size_t)endIdx].pitchOffset;
    double dt = t1 - t0;
    double dp = p1 - p0;
    double lineLen = std::sqrt(dt * dt + dp * dp);

    double maxDist = 0.0;
    int maxIdx = startIdx;

    for (int i = startIdx + 1; i < endIdx; ++i)
    {
        double ti = input[(size_t)i].time;
        double pi = input[(size_t)i].pitchOffset;

        double dist;
        if (lineLen < 1e-10)
        {
            
            dist = std::sqrt((ti - t0) * (ti - t0) + (pi - p0) * (pi - p0));
        }
        else
        {
            
            dist = std::abs((t1 - t0) * (p0 - pi) - (t0 - ti) * (p1 - p0)) / lineLen;
        }

        if (dist > maxDist)
        {
            maxDist = dist;
            maxIdx = i;
        }
    }

    if (maxDist > tolerance)
    {
        keep[(size_t)maxIdx] = true;
        rdpSimplify(input, startIdx, maxIdx, tolerance, keep);
        rdpSimplify(input, maxIdx, endIdx, tolerance, keep);
    }
}

void PitchCurveInterpolator::simplify(std::vector<PitchPoint>& points, float tolerance)
{
    if (points.size() <= 2) return;
    simplifyRange(points, 0, (int)points.size() - 1, tolerance);
}

void PitchCurveInterpolator::simplifyRange(std::vector<PitchPoint>& points, int startIdx, int endIdx, float tolerance)
{
    if (points.size() <= 2) return;
    startIdx = jmax(0, startIdx);
    endIdx = jmin((int)points.size() - 1, endIdx);
    if (endIdx - startIdx <= 1) return;

    std::vector<PitchPoint> subRange(points.begin() + startIdx, points.begin() + endIdx + 1);

    std::vector<bool> keep(subRange.size(), false);
    keep[0] = true;
    keep[subRange.size() - 1] = true;

    rdpSimplify(subRange, 0, (int)subRange.size() - 1, tolerance, keep);

    std::vector<PitchPoint> kept;
    for (size_t i = 0; i < subRange.size(); ++i)
    {
        if (keep[i])
            kept.push_back(subRange[i]);
    }

    auto before = std::vector<PitchPoint>(points.begin(), points.begin() + startIdx);
    auto after = std::vector<PitchPoint>(points.begin() + endIdx + 1, points.end());

    points.clear();
    points.insert(points.end(), before.begin(), before.end());
    points.insert(points.end(), kept.begin(), kept.end());
    points.insert(points.end(), after.begin(), after.end());
}

void PitchCurveInterpolator::mergeIntoCurve(std::vector<PitchPoint>& curve,
                                             const std::vector<PitchPoint>& overlay,
                                             double offsetTime)
{
    for (auto& op : overlay)
    {
        PitchPoint pp = op;
        pp.time += offsetTime;

        bool found = false;
        for (auto& cp : curve)
        {
            if (std::abs(cp.time - pp.time) < 0.001)
            {
                cp.pitchOffset += pp.pitchOffset;
                found = true;
                break;
            }
        }
        if (!found)
        {
            float existingPitch = interpolate(curve, pp.time);
            pp.pitchOffset += existingPitch;
            curve.push_back(pp);
        }
    }

    std::sort(curve.begin(), curve.end(),
              [](const PitchPoint& a, const PitchPoint& b) { return a.time < b.time; });
}

float NoteData::getPitchAtTime(double beatTime) const
{
    double relTime = beatTime - startBeat;
    if (relTime < 0.0 || relTime > durationBeats)
        return (float)noteNumber;

    if (pitchCurve.empty())
        return (float)noteNumber;

    return (float)noteNumber + PitchCurveInterpolator::interpolate(pitchCurve, relTime);
}

float NoteData::getAmplitudeAtTime(double beatTime) const
{
    if (amplitudeCurve.empty()) return 1.0f;

    double relTime = beatTime - startBeat;
    if (relTime <= amplitudeCurve.front().time)  return amplitudeCurve.front().value;
    if (relTime >= amplitudeCurve.back().time)   return amplitudeCurve.back().value;

    for (size_t i = 1; i < amplitudeCurve.size(); ++i)
    {
        const auto& a = amplitudeCurve[i - 1];
        const auto& b = amplitudeCurve[i];
        if (relTime <= b.time)
        {
            double span = b.time - a.time;
            if (span <= 1e-9) return b.value;
            double t = (relTime - a.time) / span;
            return (float)((1.0 - t) * a.value + t * b.value);
        }
    }
    return amplitudeCurve.back().value;
}

void NoteSequence::removeNote(int index)
{
    if (index >= 0 && index < (int)notes.size())
        notes.erase(notes.begin() + index);
}

NoteData* NoteSequence::getNoteAt(double beat, int noteNumber, double tolerance)
{
    for (auto& note : notes)
    {
        if (note.noteNumber == noteNumber &&
            beat >= note.startBeat - tolerance &&
            beat <= note.getEndBeat() + tolerance)
            return &note;
    }
    return nullptr;
}

std::vector<NoteData*> NoteSequence::getNotesInRange(double startBeat, double endBeat)
{
    std::vector<NoteData*> result;
    for (auto& note : notes)
    {
        if (note.getEndBeat() >= startBeat && note.startBeat <= endBeat)
            result.push_back(&note);
    }
    return result;
}

void NoteSequence::sortNotes()
{
    std::sort(notes.begin(), notes.end(),
              [](const NoteData& a, const NoteData& b) { return a.startBeat < b.startBeat; });
}
