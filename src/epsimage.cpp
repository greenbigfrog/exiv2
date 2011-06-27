// ***************************************************************** -*- C++ -*-
/*
 * Copyright (C) 2004-2011 Andreas Huggel <ahuggel@gmx.net>
 *
 * This program is part of the Exiv2 distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301 USA.
 */
/*
  File:      epsimage.cpp
  Version:   $Rev: 2455 $
  Author(s): Michael Ulbrich (mul) <mul@rentapacs.de>
             Volker Grabsch (vog) <vog@notjusthosting.com>
  History:   7-Mar-2011, vog: created
 */
// *****************************************************************************
#include "rcsid_int.hpp"
EXIV2_RCSID("@(#) $Id: epsimage.cpp $")

// *****************************************************************************

//#define DEBUG 1

// *****************************************************************************
// included header files
#ifdef _MSC_VER
# include "exv_msvc.h"
#else
# include "exv_conf.h"
#endif
#include "epsimage.hpp"
#include "image.hpp"
#include "basicio.hpp"
#include "convert.hpp"
#include "error.hpp"
#include "futils.hpp"

// + standard includes
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

// *****************************************************************************
namespace {

    using namespace Exiv2;

    // signature of DOS EPS
    static const std::string dosEpsSignature = "\xC5\xD0\xD3\xC6";

    // first line of EPS
    static const std::string epsFirstLine[] = {
        "%!PS-Adobe-3.0 EPSF-3.0",
        "%!PS-Adobe-3.0 EPSF-3.0 ", // OpenOffice
        "%!PS-Adobe-3.1 EPSF-3.0",  // Illustrator
    };

    // blank EPS file
    static const std::string epsBlank = "%!PS-Adobe-3.0 EPSF-3.0\n"
                                        "%%BoundingBox: 0 0 0 0\n";

    // list of all valid XMP headers
    static const struct { std::string header; std::string charset; } xmpHeadersDef[] = {

        // We do not enforce the trailing "?>" here, because the XMP specification
        // permits additional attributes after begin="..." and id="...".

        // normal headers
        {"<?xpacket begin=\"\xef\xbb\xbf\" id=\"W5M0MpCehiHzreSzNTczkc9d\"", "UTF-8"},
        {"<?xpacket begin=\"\xef\xbb\xbf\" id='W5M0MpCehiHzreSzNTczkc9d'",   "UTF-8"},
        {"<?xpacket begin='\xef\xbb\xbf' id=\"W5M0MpCehiHzreSzNTczkc9d\"",   "UTF-8"},
        {"<?xpacket begin='\xef\xbb\xbf' id='W5M0MpCehiHzreSzNTczkc9d'",     "UTF-8"},
        {"<?xpacket begin=\"\xef\xbb\xbf\" id=\"W5M0MpCehiHzreSzNTczkc9d\"", "UTF-16BE"},
        {"<?xpacket begin=\"\xef\xbb\xbf\" id='W5M0MpCehiHzreSzNTczkc9d'",   "UTF-16BE"},
        {"<?xpacket begin='\xef\xbb\xbf' id=\"W5M0MpCehiHzreSzNTczkc9d\"",   "UTF-16BE"},
        {"<?xpacket begin='\xef\xbb\xbf' id='W5M0MpCehiHzreSzNTczkc9d'",     "UTF-16BE"},
        {"<?xpacket begin=\"\xef\xbb\xbf\" id=\"W5M0MpCehiHzreSzNTczkc9d\"", "UTF-16LE"},
        {"<?xpacket begin=\"\xef\xbb\xbf\" id='W5M0MpCehiHzreSzNTczkc9d'",   "UTF-16LE"},
        {"<?xpacket begin='\xef\xbb\xbf' id=\"W5M0MpCehiHzreSzNTczkc9d\"",   "UTF-16LE"},
        {"<?xpacket begin='\xef\xbb\xbf' id='W5M0MpCehiHzreSzNTczkc9d'",     "UTF-16LE"},
        {"<?xpacket begin=\"\xef\xbb\xbf\" id=\"W5M0MpCehiHzreSzNTczkc9d\"", "UTF-32BE"},
        {"<?xpacket begin=\"\xef\xbb\xbf\" id='W5M0MpCehiHzreSzNTczkc9d'",   "UTF-32BE"},
        {"<?xpacket begin='\xef\xbb\xbf' id=\"W5M0MpCehiHzreSzNTczkc9d\"",   "UTF-32BE"},
        {"<?xpacket begin='\xef\xbb\xbf' id='W5M0MpCehiHzreSzNTczkc9d'",     "UTF-32BE"},
        {"<?xpacket begin=\"\xef\xbb\xbf\" id=\"W5M0MpCehiHzreSzNTczkc9d\"", "UTF-32LE"},
        {"<?xpacket begin=\"\xef\xbb\xbf\" id='W5M0MpCehiHzreSzNTczkc9d'",   "UTF-32LE"},
        {"<?xpacket begin='\xef\xbb\xbf' id=\"W5M0MpCehiHzreSzNTczkc9d\"",   "UTF-32LE"},
        {"<?xpacket begin='\xef\xbb\xbf' id='W5M0MpCehiHzreSzNTczkc9d'",     "UTF-32LE"},

        // deprecated headers (empty begin attribute, UTF-8 only)
        {"<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"", "UTF-8"},
        {"<?xpacket begin=\"\" id='W5M0MpCehiHzreSzNTczkc9d'",   "UTF-8"},
        {"<?xpacket begin='' id=\"W5M0MpCehiHzreSzNTczkc9d\"",   "UTF-8"},
        {"<?xpacket begin='' id='W5M0MpCehiHzreSzNTczkc9d'",     "UTF-8"},
    };

    // list of all valid XMP trailers
    static const struct { std::string trailer; bool readOnly; } xmpTrailersDef[] = {

        // We do not enforce the trailing "?>" here, because the XMP specification
        // permits additional attributes after end="...".

        {"<?xpacket end=\"r\"", true},
        {"<?xpacket end='r'",   true},
        {"<?xpacket end=\"w\"", false},
        {"<?xpacket end='w'",   false},
    };

    // closing part of all valid XMP trailers
    static const std::string xmpTrailerEndDef = "?>";

    //! Write data into temp file, taking care of errors
    static void writeTemp(BasicIo& tempIo, const char* data, size_t size)
    {
        if (size == 0) return;
        if (tempIo.write(reinterpret_cast<const byte*>(data), static_cast<long>(size)) != static_cast<long>(size)) {
            #ifndef SUPPRESS_WARNINGS
            EXV_WARNING << "Failed to write to temporary file.\n";
            #endif
            throw Error(21);
        }
    }

    //! Write data into temp file, taking care of errors
    static void writeTemp(BasicIo& tempIo, const std::string &data)
    {
        writeTemp(tempIo, data.data(), data.size());
    }

    //! Get the current write position of temp file, taking care of errors
    static size_t posTemp(BasicIo& tempIo)
    {
        const long pos = tempIo.tell();
        if (pos == -1) {
            #ifndef SUPPRESS_WARNINGS
            EXV_WARNING << "Internal error while determining current write position in temporary file.\n";
            #endif
            throw Error(21);
        }
        return pos;
    }

    //! Write an unsigned 32-bit integer into temp file, taking care of errors
    static void writeTempUInt32LE(BasicIo& tempIo, unsigned int v)
    {
        const unsigned char b[4] = {
            (v >> 0*8) & 0xFF,
            (v >> 1*8) & 0xFF,
            (v >> 2*8) & 0xFF,
            (v >> 3*8) & 0xFF,
        };
        writeTemp(tempIo, reinterpret_cast<const char*>(b), sizeof(b));
    }

    //! Read an unsigned 32-bit integer in little endian
    static unsigned int readUInt32LE(const char* data, size_t startPos)
    {
        const unsigned char* b = reinterpret_cast<const unsigned char*>(data + startPos);
        return (((((b[3] << 8) | b[2]) << 8) | b[1]) << 8) | b[0];
    }

    //! Check whether a string has a certain beginning
    static bool startsWith(const std::string& s, const std::string& start)
    {
        return s.size() >= start.size() && memcmp(s.data(), start.data(), start.size()) == 0;
    }

    //! Check whether a string contains only white space characters
    static bool onlyWhitespaces(const std::string& s)
    {
        // According to the DSC 3.0 specification, 4.4 Parsing Rules,
        // only spaces and tabs are considered to be white space characters.
        return s.find_first_not_of(" \t") == std::string::npos;
    }

    //! Convert an integer of type size_t to a decimal string
    static std::string toString(size_t size)
    {
        std::ostringstream stream;
        stream << size;
        return stream.str();
    }

    //! Read the next line of a buffer, allow for changing line ending style
    static size_t readLine(std::string& line, const char* data, size_t startPos, size_t size)
    {
        line.clear();
        size_t pos = startPos;
        // step through line
        while (pos < size && data[pos] != '\r' && data[pos] != '\n') {
            line += data[pos];
            pos++;
        }
        // skip line ending, if present
        if (pos >= size) return pos;
        pos++;
        if (pos >= size) return pos;
        if (data[pos - 1] == '\r' && data[pos] == '\n') pos++;
        return pos;
    }

    //! Read the previous line of a buffer, allow for changing line ending style
    static size_t readPrevLine(std::string& line, const char* data, size_t startPos, size_t size)
    {
        line.clear();
        size_t pos = startPos;
        if (pos > size) return pos;
        // skip line ending of previous line, if present
        if (pos <= 0) return pos;
        if (data[pos - 1] == '\r' || data[pos - 1] == '\n') {
            pos--;
            if (pos <= 0) return pos;
            if (data[pos - 1] == '\r' && data[pos] == '\n') {
                pos--;
                if (pos <= 0) return pos;
            }
        }
        // step through previous line
        while (pos >= 1 && data[pos - 1] != '\r' && data[pos - 1] != '\n') {
            pos--;
            line += data[pos];
        }
        std::reverse(line.begin(), line.end());
        return pos;
    }

    //! Find an XMP block
    static void findXmp(size_t& xmpPos, size_t& xmpSize, const char* data, size_t startPos, size_t size, bool write)
    {
        // prepare list of valid XMP headers
        std::vector<std::pair<std::string, std::string> > xmpHeaders;
        for (size_t i = 0; i < (sizeof xmpHeadersDef) / (sizeof *xmpHeadersDef); i++) {
            const std::string &charset = xmpHeadersDef[i].charset;
            std::string header(xmpHeadersDef[i].header);
            if (!convertStringCharset(header, "UTF-8", charset.c_str())) {
                throw Error(28, charset);
            }
            xmpHeaders.push_back(make_pair(header, charset));
        }

        // search for valid XMP header
        xmpSize = 0;
        for (xmpPos = startPos; xmpPos < size; xmpPos++) {
            if (data[xmpPos] != '\x00' && data[xmpPos] != '<') continue;
            for (size_t i = 0; i < xmpHeaders.size(); i++) {
                const std::string &header = xmpHeaders[i].first;
                if (xmpPos + header.size() > size) continue;
                if (memcmp(data + xmpPos, header.data(), header.size()) != 0) continue;
                #ifdef DEBUG
                EXV_DEBUG << "findXmp: Found XMP header at position: " << xmpPos << "\n";
                #endif

                // prepare list of valid XMP trailers in the charset of the header
                const std::string &charset = xmpHeaders[i].second;
                std::vector<std::pair<std::string, bool> > xmpTrailers;
                for (size_t j = 0; j < (sizeof xmpTrailersDef) / (sizeof *xmpTrailersDef); j++) {
                    std::string trailer(xmpTrailersDef[j].trailer);
                    if (!convertStringCharset(trailer, "UTF-8", charset.c_str())) {
                        throw Error(28, charset);
                    }
                    xmpTrailers.push_back(make_pair(trailer, xmpTrailersDef[j].readOnly));
                }
                std::string xmpTrailerEnd(xmpTrailerEndDef);
                if (!convertStringCharset(xmpTrailerEnd, "UTF-8", charset.c_str())) {
                    throw Error(28, charset);
                }

                // search for valid XMP trailer
                for (size_t trailerPos = xmpPos + header.size(); trailerPos < size; trailerPos++) {
                    if (data[xmpPos] != '\x00' && data[xmpPos] != '<') continue;
                    for (size_t j = 0; j < xmpTrailers.size(); j++) {
                        const std::string &trailer = xmpTrailers[j].first;
                        if (trailerPos + trailer.size() > size) continue;
                        if (memcmp(data + trailerPos, trailer.data(), trailer.size()) != 0) continue;
                        #ifdef DEBUG
                        EXV_DEBUG << "findXmp: Found XMP trailer at position: " << trailerPos << "\n";
                        #endif

                        const bool readOnly = xmpTrailers[j].second;
                        if (readOnly) {
                            #ifndef SUPPRESS_WARNINGS
                            EXV_WARNING << "Unable to handle read-only XMP metadata yet. Please provide your"
                                           " sample EPS file to the Exiv2 project: http://dev.exiv2.org/projects/exiv2\n";
                            #endif
                            throw Error(write ? 21 : 14);
                        }

                        // search for end of XMP trailer
                        for (size_t trailerEndPos = trailerPos + trailer.size(); trailerEndPos + xmpTrailerEnd.size() <= size; trailerEndPos++) {
                            if (memcmp(data + trailerEndPos, xmpTrailerEnd.data(), xmpTrailerEnd.size()) == 0) {
                                xmpSize = (trailerEndPos + xmpTrailerEnd.size()) - xmpPos;
                                return;
                            }
                        }
                        #ifndef SUPPRESS_WARNINGS
                        EXV_WARNING << "Found XMP header but incomplete XMP trailer.\n";
                        #endif
                        throw Error(write ? 21 : 14);
                    }
                }
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Found XMP header but no XMP trailer.\n";
                #endif
                throw Error(write ? 21 : 14);
            }
        }
    }

    //! Find removable XMP embeddings
    static std::vector<std::pair<size_t, size_t> > findRemovableEmbeddings(const char* data, size_t posStart, size_t posEof, size_t posEndPageSetup,
                                                                           size_t xmpPos, size_t xmpSize, bool write)
    {
        std::vector<std::pair<size_t, size_t> > removableEmbeddings;
        std::string line;
        size_t pos;

        // check after XMP
        pos = xmpPos + xmpSize;
        pos = readLine(line, data, pos, posEof);
        if (line != "") return removableEmbeddings;
        #ifdef DEBUG
        EXV_DEBUG << "findRemovableEmbeddings: Found empty line after XMP\n";
        #endif
        pos = readLine(line, data, pos, posEof);
        if (line != "%end_xml_packet") return removableEmbeddings;
        #ifdef DEBUG
        EXV_DEBUG << "findRemovableEmbeddings: Found %end_xml_packet\n";
        #endif
        size_t posEmbeddingEnd = 0;
        for (int i = 0; i < 32; i++) {
            pos = readLine(line, data, pos, posEof);
            if (line == "%end_xml_code") {
                posEmbeddingEnd = pos;
                break;
            }
        }
        if (posEmbeddingEnd == 0) return removableEmbeddings;
        #ifdef DEBUG
        EXV_DEBUG << "findRemovableEmbeddings: Found %end_xml_code\n";
        #endif

        // check before XMP
        pos = xmpPos;
        pos = readPrevLine(line, data, pos, posEof);
        if (!startsWith(line, "%begin_xml_packet: ")) return removableEmbeddings;
        #ifdef DEBUG
        EXV_DEBUG << "findRemovableEmbeddings: Found %begin_xml_packet: ...\n";
        #endif
        size_t posEmbeddingStart = posEof;
        for (int i = 0; i < 32; i++) {
            pos = readPrevLine(line, data, pos, posEof);
            if (line == "%begin_xml_code") {
                posEmbeddingStart = pos;
                break;
            }
        }
        if (posEmbeddingStart == posEof) return removableEmbeddings;
        #ifdef DEBUG
        EXV_DEBUG << "findRemovableEmbeddings: Found %begin_xml_code\n";
        #endif

        // check at EOF
        pos = posEof;
        pos = readPrevLine(line, data, pos, posEof);
        if (line == "[/EMC pdfmark") {
            // Exiftool style
            #ifdef DEBUG
            EXV_DEBUG << "findRemovableEmbeddings: Found [/EMC pdfmark\n";
            #endif
        } else if (line == "[/NamespacePop pdfmark") {
            // Photoshop style
            #ifdef DEBUG
            EXV_DEBUG << "findRemovableEmbeddings: Found /NamespacePop pdfmark\n";
            #endif
            pos = readPrevLine(line, data, pos, posEof);
            if (line != "[{nextImage} 1 dict begin /Metadata {photoshop_metadata_stream} def currentdict end /PUT pdfmark") return removableEmbeddings;
            #ifdef DEBUG
            EXV_DEBUG << "findRemovableEmbeddings: Found /PUT pdfmark\n";
            #endif
        } else {
           return removableEmbeddings;
        }

        // check whether another XMP metadata block would take precedence if this one was removed
        {
            size_t xmpPos, xmpSize;
            findXmp(xmpPos, xmpSize, data, posStart, posEndPageSetup, write);
            if (xmpSize != 0) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Second XMP metadata block interferes at position: " << xmpPos << "\n";
                #endif
                if (write) throw Error(21);
            }
        }

        removableEmbeddings.push_back(std::make_pair(posEmbeddingStart, posEmbeddingEnd));
        removableEmbeddings.push_back(std::make_pair(pos, posEof));
        #ifdef DEBUG
        const size_t n = removableEmbeddings.size();
        EXV_DEBUG << "findRemovableEmbeddings: Recognized Photoshop-style XMP embedding at "
                     "[" << removableEmbeddings[n-2].first << "," << removableEmbeddings[n-2].second << ")"
                     " with trailer "
                     "[" << removableEmbeddings[n-1].first << "," << removableEmbeddings[n-1].second << ")"
                     "\n";
        #endif
        return removableEmbeddings;
    }

    //! Unified implementation of reading and writing EPS metadata
    static void readWriteEpsMetadata(BasicIo& io, std::string& xmpPacket, bool write)
    {
        // open input file
        if (io.open() != 0) {
            throw Error(9, io.path(), strError());
        }
        IoCloser closer(io);

        // read from input file via memory map
        const char *data = reinterpret_cast<const char*>(io.mmap());

        // default positions and sizes
        const size_t size = io.size();
        size_t posEps = 0;
        size_t posEndEps = size;
        size_t posWmf = 0;
        size_t sizeWmf = 0;
        size_t posTiff = 0;
        size_t sizeTiff = 0;

        // check for DOS EPS
        const bool dosEps = (size >= dosEpsSignature.size() && memcmp(data, dosEpsSignature.data(), dosEpsSignature.size()) == 0);
        if (dosEps) {
            #ifdef DEBUG
            EXV_DEBUG << "readWriteEpsMetadata: Found DOS EPS signature\n";
            #endif
            if (size < 30) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Premature end of file after DOS EPS signature.\n";
                #endif
                throw Error(write ? 21 : 14);
            }
            posEps    = readUInt32LE(data, 4);
            posEndEps = readUInt32LE(data, 8) + posEps;
            posWmf    = readUInt32LE(data, 12);
            sizeWmf   = readUInt32LE(data, 16);
            posTiff   = readUInt32LE(data, 20);
            sizeTiff  = readUInt32LE(data, 24);
            #ifdef DEBUG
            EXV_DEBUG << "readWriteEpsMetadata: EPS section at position " << posEps << ", size " << (posEndEps - posEps) << "\n";
            EXV_DEBUG << "readWriteEpsMetadata: WMF section at position " << posWmf << ", size " << sizeWmf << "\n";
            EXV_DEBUG << "readWriteEpsMetadata: TIFF section at position " << posTiff << ", size " << sizeTiff << "\n";
            #endif
            if (!(data[28] == '\xFF' && data[29] == '\xFF')) {
                #ifdef DEBUG
                EXV_DEBUG << "readWriteEpsMetadata: DOS EPS checksum is not FFFF\n";
                #endif
            }
            if (!((posWmf == 0 && sizeWmf == 0) || (posTiff == 0 && sizeTiff == 0))) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "DOS EPS file has both WMF and TIFF section. Only one of those is allowed.\n";
                #endif
                if (write) throw Error(21);
            }
            if (sizeWmf == 0 && sizeTiff == 0) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "DOS EPS file has neither WMF nor TIFF section. Exactly one of those is required.\n";
                #endif
                if (write) throw Error(21);
            }
            if (posEps < 30 || posEndEps > size) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "DOS EPS file has invalid position (" << posEps << ") or size (" << (posEndEps - posEps) << ") for EPS section.\n";
                #endif
                throw Error(write ? 21 : 14);
            }
            if (sizeWmf != 0 && (posWmf < 30 || posWmf + sizeWmf > size)) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "DOS EPS file has invalid position (" << posWmf << ") or size (" << sizeWmf << ") for WMF section.\n";
                #endif
                if (write) throw Error(21);
            }
            if (sizeTiff != 0 && (posTiff < 30 || posTiff + sizeTiff > size)) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "DOS EPS file has invalid position (" << posTiff << ") or size (" << sizeTiff << ") for TIFF section.\n";
                #endif
                if (write) throw Error(21);
            }
        }

        // check first line
        std::string firstLine;
        const size_t posSecondLine = readLine(firstLine, data, posEps, posEndEps);
        #ifdef DEBUG
        EXV_DEBUG << "readWriteEpsMetadata: First line: " << firstLine << "\n";
        #endif
        bool matched = false;
        for (size_t i = 0; !matched && i < (sizeof epsFirstLine) / (sizeof *epsFirstLine); i++) {
            matched = (firstLine == epsFirstLine[i]);
        }
        if (!matched) {
            throw Error(3, "EPS");
        }

        // determine line ending style of the first line
        if (posSecondLine >= posEndEps) {
            #ifndef SUPPRESS_WARNINGS
            EXV_WARNING << "Premature end of file after first line.\n";
            #endif
            throw Error(write ? 21 : 14);
        }
        const std::string lineEnding(data + posEps + firstLine.size(), posSecondLine - (posEps + firstLine.size()));
        #ifdef DEBUG
        if (lineEnding == "\n") {
            EXV_DEBUG << "readWriteEpsMetadata: Line ending style: Unix (LF)\n";
        } else if (lineEnding == "\r") {
            EXV_DEBUG << "readWriteEpsMetadata: Line ending style: Mac (CR)\n";
        } else if (lineEnding == "\r\n") {
            EXV_DEBUG << "readWriteEpsMetadata: Line ending style: DOS (CR LF)\n";
        } else {
            EXV_DEBUG << "readWriteEpsMetadata: Line ending style: (unknown)\n";
        }
        #endif

        // scan comments
        size_t posLanguageLevel = posEndEps;
        size_t posContainsXmp = posEndEps;
        size_t posPages = posEndEps;
        size_t posExiv2Version = posEndEps;
        size_t posExiv2Website = posEndEps;
        size_t posEndComments = posEndEps;
        size_t posPage = posEndEps;
        size_t posEndPageSetup = posEndEps;
        size_t posPageTrailer = posEndEps;
        size_t posEof = posEndEps;
        bool implicitPage = false;
        bool photoshop = false;
        bool inDefaultsOrPrologOrSetup = false;
        bool inPageSetup = false;
        for (size_t pos = posEps; pos < posEof;) {
            const size_t startPos = pos;
            std::string line;
            pos = readLine(line, data, startPos, posEndEps);
            // implicit comments
            if (line == "%%EOF" || line == "%begin_xml_code" || !(line.size() >= 2 && line[0] == '%' && '\x21' <= line[1] && line[1] <= '\x7e')) {
                if (posEndComments == posEndEps) {
                    posEndComments = startPos;
                    #ifdef DEBUG
                    EXV_DEBUG << "readWriteEpsMetadata: Found implicit EndComments at position: " << startPos << "\n";
                    #endif
                }
            }
            if (line == "%%EOF" || line == "%begin_xml_code" || (line.size() >= 1 && line[0] != '%')) {
                if (posPage == posEndEps && posEndComments != posEndEps && !inDefaultsOrPrologOrSetup && !onlyWhitespaces(line)) {
                    posPage = startPos;
                    implicitPage = true;
                    #ifdef DEBUG
                    EXV_DEBUG << "readWriteEpsMetadata: Found implicit Page at position: " << startPos << "\n";
                    #endif
                }
                if (posEndPageSetup == posEndEps && posPage != posEndEps && !inPageSetup) {
                    posEndPageSetup = startPos;
                    #ifdef DEBUG
                    EXV_DEBUG << "readWriteEpsMetadata: Found implicit EndPageSetup at position: " << startPos << "\n";
                    #endif
                }
            }
            if (line.size() >= 1 && line[0] != '%') continue; // performance optimization
            if (line == "%%EOF" && posPageTrailer == posEndEps) {
                posPageTrailer = startPos;
                #ifdef DEBUG
                EXV_DEBUG << "readWriteEpsMetadata: Found implicit PageTrailer at position: " << startPos << "\n";
                #endif
            }
            // explicit comments
            #ifdef DEBUG
            bool significantLine = true;
            #endif
            if (posEndComments == posEndEps && posLanguageLevel == posEndEps && startsWith(line, "%%LanguageLevel:")) {
                posLanguageLevel = startPos;
            } else if (posEndComments == posEndEps && posContainsXmp == posEndEps && startsWith(line, "%ADO_ContainsXMP:")) {
                posContainsXmp = startPos;
            } else if (posEndComments == posEndEps && posPages == posEndEps && startsWith(line, "%%Pages:")) {
                posPages = startPos;
            } else if (posEndComments == posEndEps && posExiv2Version == posEndEps && startsWith(line, "%Exiv2Version:")) {
                posExiv2Version = startPos;
            } else if (posEndComments == posEndEps && posExiv2Website == posEndEps && startsWith(line, "%Exiv2Website:")) {
                posExiv2Website = startPos;
            } else if (posEndComments == posEndEps && line == "%%EndComments") {
                posEndComments = startPos;
            } else if (line == "%%BeginDefaults") {
                inDefaultsOrPrologOrSetup = true;
            } else if (line == "%%EndDefaults") {
                inDefaultsOrPrologOrSetup = false;
            } else if (line == "%%BeginProlog") {
                inDefaultsOrPrologOrSetup = true;
            } else if (line == "%%EndProlog") {
                inDefaultsOrPrologOrSetup = false;
            } else if (line == "%%BeginSetup") {
                inDefaultsOrPrologOrSetup = true;
            } else if (line == "%%EndSetup") {
                inDefaultsOrPrologOrSetup = false;
            } else if (posPage == posEndEps && startsWith(line, "%%Page:")) {
                posPage = startPos;
            } else if (line == "%%BeginPageSetup") {
                inPageSetup = true;
            } else if (posEndPageSetup == posEndEps && line == "%%EndPageSetup") {
                inPageSetup = false;
                posEndPageSetup = startPos;
            } else if (posPageTrailer == posEndEps && line == "%%PageTrailer") {
                posPageTrailer = startPos;
            } else if (startsWith(line, "%BeginPhotoshop:")) {
                photoshop = true;
            } else if (line == "%%EOF") {
                posEof = startPos;
            } else if (startsWith(line, "%%BeginDocument:")) {
                if (posEndPageSetup == posEndEps) {
                    #ifndef SUPPRESS_WARNINGS
                    EXV_WARNING << "Nested document at invalid position (before explicit or implicit EndPageSetup): " << startPos << "\n";
                    #endif
                    throw Error(write ? 21 : 14);
                }
                // TODO: Add support for nested documents!
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Nested documents are currently not supported. Found nested document at position: " << startPos << "\n";
                #endif
                throw Error(write ? 21 : 14);
            } else if (posPage != posEndEps && startsWith(line, "%%Page:")) {
                if (implicitPage) {
                    #ifndef SUPPRESS_WARNINGS
                    EXV_WARNING << "Page at position " << startPos << " conflicts with implicit page at position: " << posPage << "\n";
                    #endif
                    throw Error(write ? 21 : 14);
                }
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Unable to handle multiple PostScript pages. Found second page at position: " << startPos << "\n";
                #endif
                throw Error(write ? 21 : 14);
            } else if (startsWith(line, "%%Include")) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Unable to handle PostScript %%Include DSC comments yet. Please provide your"
                               " sample EPS file to the Exiv2 project: http://dev.exiv2.org/projects/exiv2\n";
                #endif
                throw Error(write ? 21 : 14);
            } else {
                #ifdef DEBUG
                significantLine = false;
                #endif
            }
            #ifdef DEBUG
            if (significantLine) {
                EXV_DEBUG << "readWriteEpsMetadata: Found significant line \"" << line << "\" at position: " << startPos << "\n";
            }
            #endif
        }

        // interpret comment "%ADO_ContainsXMP:"
        std::string line;
        readLine(line, data, posContainsXmp, posEndEps);
        bool containsXmp;
        if (line == "%ADO_ContainsXMP: MainFirst" || line == "%ADO_ContainsXMP:MainFirst") {
            containsXmp = true;
        } else if (line == "" || line == "%ADO_ContainsXMP: NoMain" || line == "%ADO_ContainsXMP:NoMain") {
            containsXmp = false;
        } else {
            #ifndef SUPPRESS_WARNINGS
            EXV_WARNING << "Invalid line \"" << line << "\" at position: " << posContainsXmp << "\n";
            #endif
            throw Error(write ? 21 : 14);
        }

        std::vector<std::pair<size_t, size_t> > removableEmbeddings;
        bool fixBeginXmlPacket = false;
        size_t xmpPos = posEndEps;
        size_t xmpSize = 0;
        if (containsXmp) {
            // search for XMP metadata
            findXmp(xmpPos, xmpSize, data, posEps, posEndEps, write);
            if (xmpSize == 0) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Unable to find XMP metadata as announced at position: " << posContainsXmp << "\n";
                #endif
                throw Error(write ? 21 : 14);
            }
            // check embedding of XMP metadata
            const size_t posLineAfterXmp = readLine(line, data, xmpPos + xmpSize, posEndEps);
            if (line != "") {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Unexpected " << line.size() << " bytes of data after XMP at position: " << (xmpPos + xmpSize) << "\n";
                #endif
                if (write) throw Error(21);
            }
            readLine(line, data, posLineAfterXmp, posEndEps);
            if (line == "% &&end XMP packet marker&&" || line == "%  &&end XMP packet marker&&") {
                #ifdef DEBUG
                EXV_DEBUG << "readWriteEpsMetadata: Recognized flexible XMP embedding\n";
                #endif
                const size_t posBeginXmlPacket = readPrevLine(line, data, xmpPos, posEndEps);
                if (startsWith(line, "%begin_xml_packet:")) {
                    #ifdef DEBUG
                    EXV_DEBUG << "readWriteEpsMetadata: Found %begin_xml_packet before flexible XMP embedding\n";
                    #endif
                    if (write) {
                        fixBeginXmlPacket = true;
                        xmpSize += (xmpPos - posBeginXmlPacket);
                        xmpPos = posBeginXmlPacket;
                    }
                } else if (photoshop) {
                    #ifndef SUPPRESS_WARNINGS
                    EXV_WARNING << "Missing %begin_xml_packet in Photoshop EPS at position: " << xmpPos << "\n";
                    #endif
                    if (write) throw Error(21);
                }
            } else {
                removableEmbeddings = findRemovableEmbeddings(data, posEps, posEof, posEndPageSetup, xmpPos, xmpSize, write);
                if (removableEmbeddings.empty()) {
                    #ifndef SUPPRESS_WARNINGS
                    EXV_WARNING << "Unknown XMP embedding at position: " << xmpPos << "\n";
                    #endif
                    if (write) throw Error(21);
                }
            }
        }

        if (!write) {
            // copy XMP metadata
            xmpPacket.assign(data + xmpPos, xmpSize);
        } else {
            const bool useExistingEmbedding = (xmpPos != posEndEps && removableEmbeddings.empty());

            // TODO: Add support for deleting XMP metadata. Note that this is not
            //       as simple as it may seem, and requires special attention!
            if (xmpPacket.size() == 0) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Deleting XMP metadata is currently not supported.\n";
                #endif
                throw Error(21);
            }

            // create temporary output file
            BasicIo::AutoPtr tempIo(io.temporary());
            assert (tempIo.get() != 0);
            if (!tempIo->isopen()) {
                #ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Unable to create temporary file for writing.\n";
                #endif
                throw Error(21);
            }
            #ifdef DEBUG
            EXV_DEBUG << "readWriteEpsMetadata: Created temporary file " << tempIo->path() << "\n";
            #endif

            // sort all positions
            std::vector<size_t> positions;
            positions.push_back(posLanguageLevel);
            positions.push_back(posContainsXmp);
            positions.push_back(posPages);
            positions.push_back(posExiv2Version);
            positions.push_back(posExiv2Website);
            positions.push_back(posEndComments);
            positions.push_back(posPage);
            positions.push_back(posEndPageSetup);
            positions.push_back(posPageTrailer);
            positions.push_back(posEof);
            positions.push_back(posEndEps);
            if (useExistingEmbedding) {
                positions.push_back(xmpPos);
            }
            for (std::vector<std::pair<size_t, size_t> >::const_iterator e = removableEmbeddings.begin(); e != removableEmbeddings.end(); e++) {
                positions.push_back(e->first);
            }
            std::sort(positions.begin(), positions.end());

            // assemble result EPS document
            if (dosEps) {
                // DOS EPS header will be written afterwards
                writeTemp(*tempIo, std::string(30, '\x00'));
            }
            const size_t posEpsNew = posTemp(*tempIo);
            size_t prevPos = posEps;
            size_t prevSkipPos = prevPos;
            for (std::vector<size_t>::const_iterator i = positions.begin(); i != positions.end(); i++) {
                const size_t pos = *i;
                if (pos == prevPos) continue;
                if (pos < prevSkipPos) {
                    #ifndef SUPPRESS_WARNINGS
                    EXV_WARNING << "Internal error while assembling the result EPS document: "
                                   "Unable to continue at position " << pos << " after skipping to position " << prevSkipPos << "\n";
                    #endif
                    throw Error(21);
                }
                writeTemp(*tempIo, data + prevSkipPos, pos - prevSkipPos);
                const size_t posLineEnd = readLine(line, data, pos, posEndEps);
                size_t skipPos = pos;
                // add last line ending if necessary
                if (pos == posEndEps && pos >= 1 && data[pos - 1] != '\r' && data[pos - 1] != '\n') {
                    writeTemp(*tempIo, lineEnding);
                    #ifdef DEBUG
                    EXV_DEBUG << "readWriteEpsMetadata: Added missing line ending of last line\n";
                    #endif
                }
                // update and complement DSC comments
                if (pos == posLanguageLevel && posLanguageLevel != posEndEps && !useExistingEmbedding) {
                    if (line == "%%LanguageLevel:1" || line == "%%LanguageLevel: 1") {
                        writeTemp(*tempIo, "%%LanguageLevel: 2" + lineEnding);
                        skipPos = posLineEnd;
                    }
                }
                if (pos == posContainsXmp && posContainsXmp != posEndEps) {
                    if (line != "%ADO_ContainsXMP: MainFirst") {
                        writeTemp(*tempIo, "%ADO_ContainsXMP: MainFirst" + lineEnding);
                        skipPos = posLineEnd;
                    }
                }
                if (pos == posExiv2Version && posExiv2Version != posEndEps) {
                    writeTemp(*tempIo, "%Exiv2Version: " + std::string(version()) + lineEnding);
                    skipPos = posLineEnd;
                }
                if (pos == posExiv2Website && posExiv2Website != posEndEps) {
                    writeTemp(*tempIo, "%Exiv2Website: http://www.exiv2.org/" + lineEnding);
                    skipPos = posLineEnd;
                }
                if (pos == posEndComments) {
                    if (posLanguageLevel == posEndEps && !useExistingEmbedding) {
                        writeTemp(*tempIo, "%%LanguageLevel: 2" + lineEnding);
                    }
                    if (posContainsXmp == posEndEps) {
                        writeTemp(*tempIo, "%ADO_ContainsXMP: MainFirst" + lineEnding);
                    }
                    if (posPages == posEndEps) {
                        writeTemp(*tempIo, "%%Pages: 1" + lineEnding);
                    }
                    if (posExiv2Version == posEndEps) {
                        writeTemp(*tempIo, "%Exiv2Version: " + std::string(version()) + lineEnding);
                    }
                    if (posExiv2Website == posEndEps) {
                        writeTemp(*tempIo, "%Exiv2Website: http://www.exiv2.org/" + lineEnding);
                    }
                    readLine(line, data, posEndComments, posEndEps);
                    if (line != "%%EndComments") {
                        writeTemp(*tempIo, "%%EndComments" + lineEnding);
                    }
                }
                if (pos == posPage) {
                    if (!startsWith(line, "%%Page:")) {
                        writeTemp(*tempIo, "%%Page: 1 1" + lineEnding);
                        writeTemp(*tempIo, "%%EndPageComments" + lineEnding);
                    }
                }
                // remove unflexible embeddings
                for (std::vector<std::pair<size_t, size_t> >::const_iterator e = removableEmbeddings.begin(); e != removableEmbeddings.end(); e++) {
                    if (pos == e->first) {
                        skipPos = e->second;
                        break;
                    }
                }
                if (useExistingEmbedding) {
                    // insert XMP metadata into existing flexible embedding
                    if (pos == xmpPos) {
                        if (fixBeginXmlPacket) {
                            writeTemp(*tempIo, "%begin_xml_packet: " + toString(xmpPacket.size()) + lineEnding);
                        }
                        writeTemp(*tempIo, xmpPacket.data(), xmpPacket.size());
                        skipPos += xmpSize;
                    }
                } else {
                    // insert XMP metadata with new flexible embedding
                    if (pos == posEndPageSetup) {
                        if (line != "%%EndPageSetup") {
                            writeTemp(*tempIo, "%%BeginPageSetup" + lineEnding);
                        }
                        writeTemp(*tempIo, "%Exiv2BeginXMP: Before %%EndPageSetup" + lineEnding);
                        if (photoshop) {
                            writeTemp(*tempIo, "%Exiv2Notice: The following line is needed by Photoshop." + lineEnding);
                            writeTemp(*tempIo, "%begin_xml_code" + lineEnding);
                        }
                        writeTemp(*tempIo, "/currentdistillerparams where" + lineEnding);
                        writeTemp(*tempIo, "{pop currentdistillerparams /CoreDistVersion get 5000 lt} {true} ifelse" + lineEnding);
                        writeTemp(*tempIo, "{userdict /Exiv2_pdfmark /cleartomark load put" + lineEnding);
                        writeTemp(*tempIo, "    userdict /Exiv2_metafile_pdfmark {flushfile cleartomark} bind put}" + lineEnding);
                        writeTemp(*tempIo, "{userdict /Exiv2_pdfmark /pdfmark load put" + lineEnding);
                        writeTemp(*tempIo, "    userdict /Exiv2_metafile_pdfmark {/PUT pdfmark} bind put} ifelse" + lineEnding);
                        writeTemp(*tempIo, "[/NamespacePush Exiv2_pdfmark" + lineEnding);
                        writeTemp(*tempIo, "[/_objdef {Exiv2_metadata_stream} /type /stream /OBJ Exiv2_pdfmark" + lineEnding);
                        writeTemp(*tempIo, "[{Exiv2_metadata_stream} 2 dict begin" + lineEnding);
                        writeTemp(*tempIo, "    /Type /Metadata def /Subtype /XML def currentdict end /PUT Exiv2_pdfmark" + lineEnding);
                        writeTemp(*tempIo, "[{Exiv2_metadata_stream}" + lineEnding);
                        writeTemp(*tempIo, "    currentfile 0 (% &&end XMP packet marker&&)" + lineEnding);
                        writeTemp(*tempIo, "    /SubFileDecode filter Exiv2_metafile_pdfmark" + lineEnding);
                        if (photoshop) {
                            writeTemp(*tempIo, "%Exiv2Notice: The following line is needed by Photoshop. "
                                               "Parameter must be exact size of XMP metadata." + lineEnding);
                            writeTemp(*tempIo, "%begin_xml_packet: " + toString(xmpPacket.size()) + lineEnding);
                        }
                        writeTemp(*tempIo, xmpPacket.data(), xmpPacket.size());
                        writeTemp(*tempIo, lineEnding);
                        writeTemp(*tempIo, "% &&end XMP packet marker&&" + lineEnding);
                        writeTemp(*tempIo, "[/Document 1 dict begin" + lineEnding);
                        writeTemp(*tempIo, "    /Metadata {Exiv2_metadata_stream} def currentdict end /BDC Exiv2_pdfmark" + lineEnding);
                        if (photoshop) {
                            writeTemp(*tempIo, "%Exiv2Notice: The following line is needed by Photoshop." + lineEnding);
                            writeTemp(*tempIo, "%end_xml_code" + lineEnding);
                        }
                        writeTemp(*tempIo, "%Exiv2EndXMP" + lineEnding);
                        if (line != "%%EndPageSetup") {
                            writeTemp(*tempIo, "%%EndPageSetup" + lineEnding);
                        }
                    }
                    if (pos == posPageTrailer) {
                        if (pos == posEndEps || pos == posEof) {
                            writeTemp(*tempIo, "%%PageTrailer" + lineEnding);
                        } else {
                            skipPos = posLineEnd;
                        }
                        writeTemp(*tempIo, "%Exiv2BeginXMP: After %%PageTrailer" + lineEnding);
                        writeTemp(*tempIo, "[/EMC Exiv2_pdfmark" + lineEnding);
                        writeTemp(*tempIo, "[/NamespacePop Exiv2_pdfmark" + lineEnding);
                        writeTemp(*tempIo, "%Exiv2EndXMP" + lineEnding);
                    }
                }
                // add EOF comment if necessary
                if (pos == posEndEps && posEof == posEndEps) {
                    writeTemp(*tempIo, "%%EOF" + lineEnding);
                }
                prevPos = pos;
                prevSkipPos = skipPos;
            }
            const size_t posEndEpsNew = posTemp(*tempIo);
            #ifdef DEBUG
            EXV_DEBUG << "readWriteEpsMetadata: New EPS size: " << (posEndEpsNew - posEpsNew) << "\n";
            #endif
            if (dosEps) {
                // add WMF and/or TIFF section if present
                writeTemp(*tempIo, data + posWmf, sizeWmf);
                writeTemp(*tempIo, data + posTiff, sizeTiff);
                #ifdef DEBUG
                EXV_DEBUG << "readWriteEpsMetadata: New DOS EPS total size: " << posTemp(*tempIo) << "\n";
                #endif
                // write DOS EPS header
                if (tempIo->seek(0, BasicIo::beg) != 0) {
                    #ifndef SUPPRESS_WARNINGS
                    EXV_WARNING << "Internal error while seeking in temporary file.\n";
                    #endif
                    throw Error(21);
                }
                writeTemp(*tempIo, dosEpsSignature);
                writeTempUInt32LE(*tempIo, posEpsNew);
                writeTempUInt32LE(*tempIo, posEndEpsNew - posEpsNew);
                writeTempUInt32LE(*tempIo, sizeWmf == 0 ? 0 : posEndEpsNew);
                writeTempUInt32LE(*tempIo, sizeWmf);
                writeTempUInt32LE(*tempIo, sizeTiff == 0 ? 0 : posEndEpsNew + sizeWmf);
                writeTempUInt32LE(*tempIo, sizeTiff);
                writeTemp(*tempIo, std::string("\xFF\xFF"));
            }

            // copy temporary file to real output file
            io.close();
            io.transfer(*tempIo);
        }
    }

} // namespace

// *****************************************************************************
// class member definitions
namespace Exiv2
{

    EpsImage::EpsImage(BasicIo::AutoPtr io, bool create)
            : Image(ImageType::eps, mdXmp, io)
    {
        //LogMsg::setLevel(LogMsg::debug);
        if (create) {
            if (io_->open() == 0) {
                #ifdef DEBUG
                EXV_DEBUG << "Exiv2::EpsImage:: Creating blank EPS image\n";
                #endif
                IoCloser closer(*io_);
                if (io_->write(reinterpret_cast<const byte*>(epsBlank.data()), static_cast<long>(epsBlank.size())) != static_cast<long>(epsBlank.size())) {
                    #ifndef SUPPRESS_WARNINGS
                    EXV_WARNING << "Failed to write blank EPS image.\n";
                    #endif
                    throw Error(21);
                }
            }
        }
    }

    std::string EpsImage::mimeType() const
    {
        return "application/postscript";
    }

    void EpsImage::setComment(const std::string& /*comment*/)
    {
        throw Error(32, "Image comment", "EPS");
    }

    void EpsImage::readMetadata()
    {
        #ifdef DEBUG
        EXV_DEBUG << "Exiv2::EpsImage::readMetadata: Reading EPS file " << io_->path() << "\n";
        #endif

        // read metadata
        readWriteEpsMetadata(*io_, xmpPacket_, /* write = */ false);

        // decode XMP metadata
        if (xmpPacket_.size() > 0 && XmpParser::decode(xmpData_, xmpPacket_) > 1) {
            #ifndef SUPPRESS_WARNINGS
            EXV_WARNING << "Failed to decode XMP metadata.\n";
            #endif
            throw Error(14);
        }

        #ifdef DEBUG
        EXV_DEBUG << "Exiv2::EpsImage::readMetadata: Finished reading EPS file " << io_->path() << "\n";
        #endif
    }

    void EpsImage::writeMetadata()
    {
        #ifdef DEBUG
        EXV_DEBUG << "Exiv2::EpsImage::writeMetadata: Writing EPS file " << io_->path() << "\n";
        #endif

        // encode XMP metadata if necessary
        if (!writeXmpFromPacket() && XmpParser::encode(xmpPacket_, xmpData_) > 1) {
            #ifndef SUPPRESS_WARNINGS
            EXV_WARNING << "Failed to encode XMP metadata.\n";
            #endif
            throw Error(21);
        }

        // write metadata
        readWriteEpsMetadata(*io_, xmpPacket_, /* write = */ true);

        #ifdef DEBUG
        EXV_DEBUG << "Exiv2::EpsImage::writeMetadata: Finished writing EPS file " << io_->path() << "\n";
        #endif
    }

    // *************************************************************************
    // free functions
    Image::AutoPtr newEpsInstance(BasicIo::AutoPtr io, bool create)
    {
        Image::AutoPtr image(new EpsImage(io, create));
        if (!image->good()) {
            image.reset();
        }
        return image;
    }

    bool isEpsType(BasicIo& iIo, bool advance)
    {
        // read as many bytes as needed for the longest (DOS) EPS signature
        long bufSize = dosEpsSignature.size();
        for (size_t i = 0; i < (sizeof epsFirstLine) / (sizeof *epsFirstLine); i++) {
            if (bufSize < static_cast<long>(epsFirstLine[i].size())) {
                bufSize = static_cast<long>(epsFirstLine[i].size());
            }
        }
        DataBuf buf = iIo.read(bufSize);
        if (iIo.error() || buf.size_ != bufSize) {
            return false;
        }
        // check for all possible (DOS) EPS signatures
        bool matched = (memcmp(buf.pData_, dosEpsSignature.data(), dosEpsSignature.size()) == 0);
        for (size_t i = 0; !matched && i < (sizeof epsFirstLine) / (sizeof *epsFirstLine); i++) {
            matched = (memcmp(buf.pData_, epsFirstLine[i].data(), epsFirstLine[i].size()) == 0);
        }
        // seek back if possible and requested
        if (!advance || !matched) {
            iIo.seek(-buf.size_, BasicIo::cur);
        }
        return matched;
    }

} // namespace Exiv2