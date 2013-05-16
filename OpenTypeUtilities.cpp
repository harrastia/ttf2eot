/* Modified for use with ttf2eot, originally from WebKit. */

/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <cstddef>
#include <string.h>
#include <vector>

#ifndef _MSC_VER
# include <stdint.h>
#else
typedef unsigned char uint8_t;
#endif

#include "OpenTypeUtilities.h"


using std::vector;

typedef unsigned Fixed;

#define DEFAULT_CHARSET 1

struct BigEndianUShort { 
    operator unsigned short() const { return (v & 0x00ff) << 8 | v >> 8; }
    BigEndianUShort(unsigned short u) : v((u & 0x00ff) << 8 | u >> 8) { }
    unsigned short v;
};

struct BigEndianULong { 
    operator unsigned() const { return (v & 0xff) << 24 | (v & 0xff00) << 8 | (v & 0xff0000) >> 8 | v >> 24; }
    BigEndianULong(unsigned u) : v((u & 0xff) << 24 | (u & 0xff00) << 8 | (u & 0xff0000) >> 8 | u >> 24) { }
    unsigned v;
};

#pragma pack(1)

struct EOTPrefix {
    unsigned eotSize;
    unsigned fontDataSize;
    unsigned version;
    unsigned flags;
    uint8_t fontPANOSE[10];
    uint8_t charset;
    uint8_t italic;
    unsigned weight;
    unsigned short fsType;
    unsigned short magicNumber;
    unsigned unicodeRange[4];
    unsigned codePageRange[2];
    unsigned checkSumAdjustment;
    unsigned reserved[4];
    unsigned short padding1;
};

struct TableDirectoryEntry {
    BigEndianULong tag;
    BigEndianULong checkSum;
    BigEndianULong offset;
    BigEndianULong length;
};

struct sfntHeader {
    Fixed version;
    BigEndianUShort numTables;
    BigEndianUShort searchRange;
    BigEndianUShort entrySelector;
    BigEndianUShort rangeShift;
    TableDirectoryEntry tables[1];
};

struct OS2Table {
    BigEndianUShort version;
    BigEndianUShort avgCharWidth;
    BigEndianUShort weightClass;
    BigEndianUShort widthClass;
    BigEndianUShort fsType;
    BigEndianUShort subscriptXSize;
    BigEndianUShort subscriptYSize;
    BigEndianUShort subscriptXOffset;
    BigEndianUShort subscriptYOffset;
    BigEndianUShort superscriptXSize;
    BigEndianUShort superscriptYSize;
    BigEndianUShort superscriptXOffset;
    BigEndianUShort superscriptYOffset;
    BigEndianUShort strikeoutSize;
    BigEndianUShort strikeoutPosition;
    BigEndianUShort familyClass;
    uint8_t panose[10];
    BigEndianULong unicodeRange[4];
    uint8_t vendID[4];
    BigEndianUShort fsSelection;
    BigEndianUShort firstCharIndex;
    BigEndianUShort lastCharIndex;
    BigEndianUShort typoAscender;
    BigEndianUShort typoDescender;
    BigEndianUShort typoLineGap;
    BigEndianUShort winAscent;
    BigEndianUShort winDescent;
    BigEndianULong codePageRange[2];
    BigEndianUShort xHeight;
    BigEndianUShort capHeight;
    BigEndianUShort defaultChar;
    BigEndianUShort breakChar;
    BigEndianUShort maxContext;
};

struct headTable {
    Fixed version;
    Fixed fontRevision;
    BigEndianULong checkSumAdjustment;
    BigEndianULong magicNumber;
    BigEndianUShort flags;
    BigEndianUShort unitsPerEm;
    long long created;
    long long modified;
    BigEndianUShort xMin;
    BigEndianUShort xMax;
    BigEndianUShort yMin;
    BigEndianUShort yMax;
    BigEndianUShort macStyle;
    BigEndianUShort lowestRectPPEM;
    BigEndianUShort fontDirectionHint;
    BigEndianUShort indexToLocFormat;
    BigEndianUShort glyphDataFormat;
};

struct nameRecord {
    BigEndianUShort platformID;
    BigEndianUShort encodingID;
    BigEndianUShort languageID;
    BigEndianUShort nameID;
    BigEndianUShort length;
    BigEndianUShort offset;
};

struct nameTable {
    BigEndianUShort format;
    BigEndianUShort count;
    BigEndianUShort stringOffset;
    nameRecord nameRecords[1];
};

#pragma pack()

static void appendBigEndianStringToEOTHeader(vector<uint8_t>&eotHeader, const BigEndianUShort* string, unsigned short length)
{
    size_t size = eotHeader.size();
    eotHeader.resize(size + length + 2 * sizeof(unsigned short));
    unsigned short* dst = reinterpret_cast<unsigned short*>(&eotHeader[0] + size);
    unsigned i = 0;
    dst[i++] = length;
    unsigned numCharacters = length / 2;
    for (unsigned j = 0; j < numCharacters; j++)
        dst[i++] = string[j];
    dst[i] = 0;
}

bool getEOTHeader(unsigned char* fontData, size_t fontSize, vector<uint8_t>& eotHeader, size_t& overlayDst, size_t& overlaySrc, size_t& overlayLength)
{
    overlayDst = 0;
    overlaySrc = 0;
    overlayLength = 0;

    size_t dataLength = fontSize;
    const char* data = (const char *) fontData;

    eotHeader.resize(sizeof(EOTPrefix));
    EOTPrefix* prefix = reinterpret_cast<EOTPrefix*>(&eotHeader[0]);

    prefix->fontDataSize = dataLength;
    prefix->version = 0x00020001;
    prefix->flags = 0;

    if (dataLength < offsetof(sfntHeader, tables))
        return false;

    const sfntHeader* sfnt = reinterpret_cast<const sfntHeader*>(data);

    if (dataLength < offsetof(sfntHeader, tables) + sfnt->numTables * sizeof(TableDirectoryEntry))
        return false;

    bool haveOS2 = false;
    bool haveHead = false;
    bool haveName = false;

    const BigEndianUShort* familyName = 0;
    unsigned short familyNameLength = 0;
    const BigEndianUShort* subfamilyName = 0;
    unsigned short subfamilyNameLength = 0;
    const BigEndianUShort* fullName = 0;
    unsigned short fullNameLength = 0;
    const BigEndianUShort* versionString = 0;
    unsigned short versionStringLength = 0;

    for (unsigned i = 0; i < sfnt->numTables; i++) {
        unsigned tableOffset = sfnt->tables[i].offset;
        unsigned tableLength = sfnt->tables[i].length;

        if (dataLength < tableOffset || dataLength < tableLength || dataLength < tableOffset + tableLength)
            return false;

        unsigned tableTag = sfnt->tables[i].tag;
        switch (tableTag) {
            case 'OS/2':
                {
                    if (dataLength < tableOffset + sizeof(OS2Table))
                        return false;

                    haveOS2 = true;
                    const OS2Table* OS2 = reinterpret_cast<const OS2Table*>(data + tableOffset);
                    for (unsigned j = 0; j < 10; j++)
                        prefix->fontPANOSE[j] = OS2->panose[j];
                    prefix->italic = OS2->fsSelection & 0x01;
                    prefix->weight = OS2->weightClass;
                    // FIXME: Should use OS2->fsType, but some TrueType fonts set it to an over-restrictive value.
                    // Since ATS does not enforce this on Mac OS X, we do not enforce it either.
                    prefix->fsType = 0;            
                    for (unsigned j = 0; j < 4; j++)
                        prefix->unicodeRange[j] = OS2->unicodeRange[j];
                    for (unsigned j = 0; j < 2; j++)
                        prefix->codePageRange[j] = OS2->codePageRange[j];
                    break;
                }
            case 'head':
                {
                    if (dataLength < tableOffset + sizeof(headTable))
                        return false;

                    haveHead = true;
                    const headTable* head = reinterpret_cast<const headTable*>(data + tableOffset);
                    prefix->checkSumAdjustment = head->checkSumAdjustment;
                    break;
                }
            case 'name':
                {
                    if (dataLength < tableOffset + offsetof(nameTable, nameRecords))
                        return false;

                    haveName = true;
                    const nameTable* name = reinterpret_cast<const nameTable*>(data + tableOffset);
                    for (int j = 0; j < name->count; j++) {
                        if (dataLength < tableOffset + offsetof(nameTable, nameRecords) + (j + 1) * sizeof(nameRecord))
                            return false;
                        if (name->nameRecords[j].platformID == 3 && name->nameRecords[j].encodingID == 1 && name->nameRecords[j].languageID == 0x0409) {
                            if (dataLength < tableOffset + name->stringOffset + name->nameRecords[j].offset + name->nameRecords[j].length)
                                return false;

                            unsigned short nameLength = name->nameRecords[j].length;
                            const BigEndianUShort* nameString = reinterpret_cast<const BigEndianUShort*>(data + tableOffset + name->stringOffset + name->nameRecords[j].offset);
                            
                            switch (name->nameRecords[j].nameID) {
                                case 1:
                                    familyNameLength = nameLength;
                                    familyName = nameString;
                                    break;
                                case 2:
                                    subfamilyNameLength = nameLength;
                                    subfamilyName = nameString;
                                    break;
                                case 4:
                                    fullNameLength = nameLength;
                                    fullName = nameString;
                                    break;
                                case 5:
                                    versionStringLength = nameLength;
                                    versionString = nameString;
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                    break;
                }
            default:
                break;
        }
        if (haveOS2 && haveHead && haveName)
            break;
    }

    prefix->charset = DEFAULT_CHARSET;
    prefix->magicNumber = 0x504c;
    prefix->reserved[0] = 0;
    prefix->reserved[1] = 0;
    prefix->reserved[2] = 0;
    prefix->reserved[3] = 0;
    prefix->padding1 = 0;

    appendBigEndianStringToEOTHeader(eotHeader, familyName, familyNameLength);
    appendBigEndianStringToEOTHeader(eotHeader, subfamilyName, subfamilyNameLength);
    appendBigEndianStringToEOTHeader(eotHeader, versionString, versionStringLength);

    // If possible, ensure that the family name is a prefix of the full name.
    if (fullNameLength >= familyNameLength && memcmp(familyName, fullName, familyNameLength)) {
        overlaySrc = reinterpret_cast<const char*>(fullName) - data;
        overlayDst = reinterpret_cast<const char*>(familyName) - data;
        overlayLength = familyNameLength;
    }

    appendBigEndianStringToEOTHeader(eotHeader, fullName, fullNameLength);

    unsigned short padding = 0;
    eotHeader.push_back(padding);
    eotHeader.push_back(padding);

    prefix = reinterpret_cast<EOTPrefix*>(&eotHeader[0]);
    prefix->eotSize = eotHeader.size() + fontSize;

    return true;
}
