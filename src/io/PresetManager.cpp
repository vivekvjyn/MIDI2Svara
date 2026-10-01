#include "PresetManager.h"

XmlElement PresetManager::noteToXml(const NoteData& note)
{
    XmlElement xml("Note");
    xml.setAttribute("noteNumber", note.noteNumber);
    xml.setAttribute("velocity", (double)note.velocity);
    xml.setAttribute("startBeat", note.startBeat);
    xml.setAttribute("durationBeats", note.durationBeats);

    for (auto& pt : note.pitchCurve)
        xml.addChildElement(new XmlElement(pitchPointToXml(pt)));

    return xml;
}

NoteData PresetManager::xmlToNote(const XmlElement& xml)
{
    NoteData note;
    note.noteNumber = xml.getIntAttribute("noteNumber", 60);
    note.velocity = (float)xml.getDoubleAttribute("velocity", 0.8);
    note.startBeat = xml.getDoubleAttribute("startBeat", 0.0);
    note.durationBeats = xml.getDoubleAttribute("durationBeats", 1.0);

    for (int i = 0; i < xml.getNumChildElements(); ++i)
    {
        auto* child = xml.getChildElement(i);
        if (child != nullptr && child->getTagName() == "PitchPoint")
            note.pitchCurve.push_back(xmlToPitchPoint(*child));
    }

    return note;
}

XmlElement PresetManager::pitchPointToXml(const PitchPoint& pt)
{
    XmlElement xml("PitchPoint");
    xml.setAttribute("time", pt.time);
    xml.setAttribute("pitchOffset", pt.pitchOffset);
    xml.setAttribute("bias", pt.bias);
    xml.setAttribute("curvature", (double)pt.curvature);
    xml.setAttribute("curveType", (int)pt.curveType);
    return xml;
}

PitchPoint PresetManager::xmlToPitchPoint(const XmlElement& xml)
{
    PitchPoint pt;
    pt.time = xml.getDoubleAttribute("time", 0.0);
    pt.pitchOffset = xml.getDoubleAttribute("pitchOffset", 0.0);
    pt.bias = xml.getDoubleAttribute("bias", 0.0);
    pt.curvature = (float)xml.getDoubleAttribute("curvature", 1.0);
    pt.curveType = (PitchPoint::CurveType)xml.getIntAttribute("curveType", 0);
    return pt;
}

XmlElement PresetManager::vibratoToXml(const SegmentVibrato& vib)
{
    XmlElement xml("Vibrato");
    xml.setAttribute("enabled", vib.enabled);
    xml.setAttribute("depth", (double)vib.depth);
    xml.setAttribute("rate", (double)vib.rate);
    xml.setAttribute("fadeInFrac", (double)vib.fadeInFrac);
    xml.setAttribute("fadeOutFrac", (double)vib.fadeOutFrac);
    xml.setAttribute("offset", (double)vib.offset);
    xml.setAttribute("waveform", (int)vib.waveform);
    return xml;
}

SegmentVibrato PresetManager::xmlToVibrato(const XmlElement& xml)
{
    SegmentVibrato vib;
    vib.enabled = xml.getBoolAttribute("enabled", false);
    vib.depth = (float)xml.getDoubleAttribute("depth", 0.2);
    vib.rate = (float)xml.getDoubleAttribute("rate", 5.0);
    vib.fadeInFrac = (float)xml.getDoubleAttribute("fadeInFrac", 0.15);
    vib.fadeOutFrac = (float)xml.getDoubleAttribute("fadeOutFrac", 0.15);
    vib.offset = (float)xml.getDoubleAttribute("offset", 0.0);
    vib.waveform = (VibratoWaveform)xml.getIntAttribute("waveform", 0);
    return vib;
}

XmlElement PresetManager::stateToXml(const State& state)
{
    XmlElement xml("FluidPitch2Preset");
    xml.setAttribute("version", 1);
    xml.setAttribute("bpm", state.bpm);
    xml.setAttribute("masterVolume", (double)state.masterVolume);
    xml.setAttribute("midiOutputMode", state.midiOutputMode);
    xml.setAttribute("midiPitchBendRange", state.midiPitchBendRange);
    xml.setAttribute("viewStartBeat", state.viewStartBeat);
    xml.setAttribute("viewEndBeat", state.viewEndBeat);
    xml.setAttribute("viewLowest", state.viewLowest);
    xml.setAttribute("viewHighest", state.viewHighest);

    auto* notesXml = xml.createNewChildElement("Notes");
    for (auto& note : state.notes)
        notesXml->addChildElement(new XmlElement(noteToXml(note)));

    return xml;
}

PresetManager::State PresetManager::xmlToState(const XmlElement& xml)
{
    State state;
    state.bpm = xml.getDoubleAttribute("bpm", 120.0);
    state.masterVolume = (float)xml.getDoubleAttribute("masterVolume", 0.8);
    state.midiOutputMode = xml.getIntAttribute("midiOutputMode", 0);
    state.midiPitchBendRange = xml.getIntAttribute("midiPitchBendRange", 48);
    state.viewStartBeat = xml.getDoubleAttribute("viewStartBeat", 0.0);
    state.viewEndBeat = xml.getDoubleAttribute("viewEndBeat", 16.0);
    state.viewLowest = xml.getDoubleAttribute("viewLowest", 36.0);
    state.viewHighest = xml.getDoubleAttribute("viewHighest", 84.0);

    if (auto* notesXml = xml.getChildByName("Notes"))
    {
        for (int i = 0; i < notesXml->getNumChildElements(); ++i)
        {
            auto* child = notesXml->getChildElement(i);
            if (child != nullptr && child->getTagName() == "Note")
                state.notes.push_back(xmlToNote(*child));
        }
    }

    return state;
}

void PresetManager::stateToBinary(const State& state, MemoryBlock& destData)
{
    auto xml = stateToXml(state);
    MemoryOutputStream stream(destData, false);
    xml.writeTo(stream, XmlElement::TextFormat());
}

PresetManager::State PresetManager::binaryToState(const void* data, int sizeInBytes)
{
    MemoryInputStream stream(data, (size_t)sizeInBytes, false);
    auto xml = XmlDocument::parse(stream.readEntireStreamAsString());
    if (xml != nullptr && xml->hasTagName("FluidPitch2Preset"))
        return xmlToState(*xml);
    return {};
}

bool PresetManager::saveToFile(const State& state, const File& file)
{
    auto xml = stateToXml(state);
    return xml.writeTo(file);
}

PresetManager::State PresetManager::loadFromFile(const File& file, bool& success)
{
    auto xml = XmlDocument::parse(file.loadFileAsString());
    if (xml != nullptr && xml->hasTagName("FluidPitch2Preset"))
    {
        success = true;
        return xmlToState(*xml);
    }
    success = false;
    return {};
}

File PresetManager::getPresetDirectory()
{
    auto dir = File::getSpecialLocation(File::userDocumentsDirectory)
                   .getChildFile("FluidPitch2")
                   .getChildFile("Presets");
    dir.createDirectory();
    return dir;
}
