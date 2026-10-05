#include "Shapes.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr int samples = 512;
    constexpr double pi = 3.14159265358979323846;

    std::vector<double> unitGrid()
    {
        std::vector<double> u((size_t)samples);
        for (int i = 0; i < samples; ++i)
            u[(size_t)i] = (double)i / (double)(samples - 1);
        return u;
    }

    double sat(double value)
    {
        return 1.0 / (1.0 + std::exp(-std::clamp(value, -60.0, 60.0)));
    }

    std::vector<double> swing(const std::vector<double>& u, double length,
                              double top, double bottom, double rate)
    {
        std::vector<double> out((size_t)samples);
        for (int i = 0; i < samples; ++i)
        {
            double phase = rate * length * u[(size_t)i];
            double wave = (1.0 - std::cos(2.0 * pi * phase)) / 2.0;
            out[(size_t)i] = top + (bottom - top) * wave;
        }
        return out;
    }

    std::vector<double> vali(const std::vector<double>& u, double length,
                             double top, double bottom, double rate)
    {
        std::vector<double> out((size_t)samples);
        for (int i = 0; i < samples; ++i)
        {
            double phase = rate * length * u[(size_t)i];
            double wave = std::abs(2.0 * (phase - std::floor(phase + 0.5)));
            out[(size_t)i] = top + (bottom - top) * wave;
        }
        return out;
    }

    std::vector<double> sphurita(const std::vector<double>& u, double length, double dip)
    {
        const double at = 0.6, span = 0.16, rate = 40.0;
        const double ramp = rate * length;
        std::vector<double> out((size_t)samples);
        for (int i = 0; i < samples; ++i)
        {
            double p = u[(size_t)i];
            double down = sat(ramp * (p - (at - span))) - sat(ramp * (p - at));
            double up = sat(ramp * (p - at)) - sat(ramp * (p - (at + span)));
            out[(size_t)i] = -dip * down + 0.6 * dip * up;
        }
        return out;
    }

    std::vector<double> ahata(const std::vector<double>& u, double length, double top)
    {
        const double at = 0.5;
        const double spread = std::max(0.12, 1e-3);
        (void)length;
        std::vector<double> out((size_t)samples);
        for (int i = 0; i < samples; ++i)
        {
            double p = (u[(size_t)i] - at) / spread;
            out[(size_t)i] = top * std::exp(-0.5 * p * p);
        }
        return out;
    }

    std::vector<double> khandippu(const std::vector<double>& u, double length, double bottom)
    {
        const double at = 0.5;
        const double spread = std::max(0.08, 1e-3);
        (void)length;
        std::vector<double> out((size_t)samples);
        for (int i = 0; i < samples; ++i)
        {
            double p = (u[(size_t)i] - at) / spread;
            out[(size_t)i] = bottom * std::exp(-0.5 * p * p);
        }
        return out;
    }

    std::vector<double> odukkal(const std::vector<double>& u, double length, double bottom)
    {
        const double at = 0.2, rate = 40.0;
        std::vector<double> out((size_t)samples);
        for (int i = 0; i < samples; ++i)
            out[(size_t)i] = bottom * sat(rate * length * (u[(size_t)i] - at));
        return out;
    }

    std::vector<double> janta(const std::vector<double>& u, double length, double top)
    {
        const double at = 0.5, rate = 40.0, span = 0.09;
        const double ramp = rate * length;
        std::vector<double> out((size_t)samples);
        for (int i = 0; i < samples; ++i)
        {
            double p = u[(size_t)i];
            double first = sat(ramp * (p - (at - 0.18 - span)))
                         - sat(ramp * (p - (at - 0.18 + span)));
            double second = sat(ramp * (p - (at + 0.18 - span)))
                          - sat(ramp * (p - (at + 0.18 + span)));
            out[(size_t)i] = top * (first + second);
        }
        return out;
    }

    std::vector<double> orikai(const std::vector<double>& u, double length, double top)
    {
        const double at = 0.75, rate = 40.0, span = 0.07;
        const double ramp = rate * length;
        std::vector<double> out((size_t)samples);
        for (int i = 0; i < samples; ++i)
        {
            double p = u[(size_t)i];
            out[(size_t)i] = top * (sat(ramp * (p - (at - span)))
                                  - sat(ramp * (p - (at + span))));
        }
        return out;
    }

    std::vector<double> tripuchcha(const std::vector<double>& u, double length, double dip)
    {
        const double at = 0.6, rate = 40.0, span = 0.12;
        const double ramp = rate * length;
        std::vector<double> out((size_t)samples, 0.0);
        for (double centre : { at - 0.16, at + 0.16 })
        {
            for (int i = 0; i < samples; ++i)
            {
                double p = u[(size_t)i];
                double down = sat(ramp * (p - (centre - span))) - sat(ramp * (p - centre));
                double up = sat(ramp * (p - centre)) - sat(ramp * (p - (centre + span)));
                out[(size_t)i] += -dip * down + 0.6 * dip * up;
            }
        }
        return out;
    }

    std::vector<double> ravai(const std::vector<double>& u, double length, double bottom)
    {
        const double at = 0.5, hold = 0.4;
        (void)length;
        const double span = std::max(hold, 1e-3);
        const double edge = 8.0 / span;
        std::vector<double> out((size_t)samples);
        for (int i = 0; i < samples; ++i)
        {
            double p = u[(size_t)i];
            out[(size_t)i] = bottom * (sat(edge * (p - (at - span / 2.0)))
                                     - sat(edge * (p - (at + span / 2.0))));
        }
        return out;
    }

    std::vector<double> ease(const std::vector<double>& u, double at, double rate)
    {
        std::vector<double> raw((size_t)samples);
        for (int i = 0; i < samples; ++i)
            raw[(size_t)i] = 1.0 / (1.0 + std::exp(-std::clamp(rate * (u[(size_t)i] - at), -60.0, 60.0)));
        const double first = raw.front();
        const double span = raw.back() - first;
        for (auto& value : raw)
            value = (value - first) / span;
        return raw;
    }

    double reachFactor(const String& gamaka)
    {
        if (gamaka == "ahata" || gamaka == "pratyahata" || gamaka == "andola") return 1.3;
        if (gamaka == "vali") return 0.76;
        if (gamaka == "khandippu") return 0.78;
        if (gamaka == "odukkal") return 1.05;
        if (gamaka == "kampita") return 1.19;
        if (gamaka == "ravai") return 0.87;
        if (gamaka == "orikai") return 1.16;
        if (gamaka == "tripuchcha") return 0.59;
        if (gamaka == "janta") return 1.0;
        return 1.0;
    }

    double interpAt(const std::vector<PitchPoint>& points, double time)
    {
        if (points.empty())
            return 0.0;
        if (time <= points.front().time)
            return points.front().pitchOffset;
        if (time >= points.back().time)
            return points.back().pitchOffset;

        auto upper = std::upper_bound(points.begin(), points.end(), time,
                                      [](double value, const PitchPoint& point)
                                      { return value < point.time; });
        const auto& b = *upper;
        const auto& a = *(upper - 1);
        double span = b.time - a.time;
        if (span <= 1.0e-12)
            return b.pitchOffset;
        return a.pitchOffset + (b.pitchOffset - a.pitchOffset) * (time - a.time) / span;
    }
}

void Shapes::transformRow(const String& raga, LookupRow& row, double here)
{
    double factor = 1.3;
    if (row.gamaka == "jaru")
        factor = 2.0;
    else if (raga == "kalyani")
        factor = 1.17;
    double shift = (raga == "sahana" || raga == "begada") ? -35.0 : 0.0;
    row.top = here + factor * (row.top - here) + shift;
    row.bottom = here + factor * (row.bottom - here) + shift;
}

std::vector<double> Shapes::curve(const LookupRow& row, double length,
                                       double here, double start)
{
    const auto u = unitGrid();
    const String& gamaka = row.gamaka;
    const double top = row.top;
    const double bottom = row.bottom;

    if (std::isnan(start))
        start = here + (double)row.previousInterval;
    start = std::min(std::max(start, bottom), top);

    const double up = top - here;
    const double down = here - bottom;
    const double reach = 1.3 * reachFactor(gamaka);
    const double span = std::max(length, 1e-9);

    const auto slide = ease(u, 0.1 / span, 120.0 * span);
    const int cycles = std::max(1, (int)std::round(3.0 * span));

    std::vector<double> base((size_t)samples);
    std::vector<double> wobble((size_t)samples);
    for (int i = 0; i < samples; ++i)
    {
        base[(size_t)i] = start + (here - start) * slide[(size_t)i];
        wobble[(size_t)i] = 10.0 * std::sin(2.0 * pi * (double)cycles * u[(size_t)i]);
    }

    std::vector<double> shape;

    if (gamaka == "sthira" || gamaka == "jaru" || gamaka == "nokku")
    {
        if (gamaka == "jaru")
        {
            const auto glide = ease(u, 0.55, 120.0 * span);
            for (int i = 0; i < samples; ++i)
                base[(size_t)i] = start + (here - start) * glide[(size_t)i];
        }
        for (int i = 0; i < samples; ++i)
            base[(size_t)i] += wobble[(size_t)i];
        return base;
    }

    if (gamaka == "kampita" || gamaka == "andola" || gamaka == "vali")
    {
        const double rate = gamaka == "kampita" ? 4.0 : gamaka == "andola" ? 1.5 : 3.0;
        const double swingTop = here + reach * (top - here);
        const double swingBottom = here - reach * (here - bottom);
        auto way = (gamaka == "vali")
            ? vali(u, length, swingTop, swingBottom, rate)
            : swing(u, length, swingTop, swingBottom, rate);
        for (int i = 0; i < samples; ++i)
        {
            double window = std::sin(pi * u[(size_t)i]);
            base[(size_t)i] += window * (way[(size_t)i] - base[(size_t)i]) + wobble[(size_t)i];
        }
        return base;
    }

    if (gamaka == "sphurita")
        shape = sphurita(u, length, down * reach);
    else if (gamaka == "tripuchcha")
        shape = tripuchcha(u, length, down * reach);
    else if (gamaka == "ahata" || gamaka == "pratyahata")
        shape = ahata(u, length, (up > 0.0 ? up : down) * reach);
    else if (gamaka == "khandippu")
        shape = khandippu(u, length, (down > 0.0 ? -down : up) * reach);
    else if (gamaka == "odukkal")
        shape = odukkal(u, length, (down > 0.0 ? -down : up) * reach);
    else if (gamaka == "janta")
        shape = janta(u, length, (up >= down ? up : -down) * reach);
    else if (gamaka == "orikai")
        shape = orikai(u, length, up * reach);
    else
        shape = ravai(u, length, (bottom - here) * reach);

    for (int i = 0; i < samples; ++i)
    {
        double gesture = shape[(size_t)i] - shape.front() * (1.0 - u[(size_t)i])
                                       - shape.back() * u[(size_t)i];
        base[(size_t)i] += gesture + wobble[(size_t)i];
    }
    return base;
}

void Shapes::connect(std::vector<NoteData>& notes)
{
    if (notes.size() < 2)
        return;

    double shortest = std::numeric_limits<double>::max();
    for (const auto& note : notes)
        if (note.durationBeats > 0.0)
            shortest = std::min(shortest, note.durationBeats);
    if (shortest >= std::numeric_limits<double>::max())
        return;

    const double half = std::min(0.06, 0.4 * shortest) / 2.0;

    for (size_t i = 1; i < notes.size(); ++i)
    {
        auto& previous = notes[i - 1];
        auto& current = notes[i];
        if (previous.pitchCurve.empty() || current.pitchCurve.empty())
            continue;

        const double boundary = current.startBeat;
        const double low = std::max(boundary - half, 0.0);
        const double high = boundary + half;
        if (high <= low)
            continue;

        const double left = interpAt(previous.pitchCurve, low - previous.startBeat);
        const double right = interpAt(current.pitchCurve, high - current.startBeat);

        auto blend = [&](PitchPoint& point, double absolute)
        {
            double u = (absolute - low) / (high - low);
            point.pitchOffset = left + (right - left) * (1.0 - std::cos(pi * u)) / 2.0;
        };

        for (auto& point : previous.pitchCurve)
        {
            double absolute = point.time + previous.startBeat;
            if (absolute >= low && absolute <= boundary)
                blend(point, absolute);
        }
        for (auto& point : current.pitchCurve)
        {
            double absolute = point.time + current.startBeat;
            if (absolute >= boundary && absolute <= high)
                blend(point, absolute);
        }
    }
}
