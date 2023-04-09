// This file is part of Columns++ for Notepad++.
// Copyright 2023 by Randall Joseph Fellmy <software@coises.com>, <http://www.coises.com/software/>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "ColumnsPlusPlus.h"
#include <fstream>
#include <iostream>
#include <regex>
#include "Shlwapi.h"

static const std::regex configurationHeader("(?:\\xEF\\xBB\\xBF)?\\s*Notepad\\+\\+\\s+Columns\\+\\+\\s+Configuration\\s+(\\d+)\\s+(\\d+)\\s*", std::regex::icase | std::regex::optimize);
static const std::regex lastSettingsHeader("\\s*LastSettings\\s*", std::regex::icase | std::regex::optimize);
static const std::regex profileHeader("\\s*Profile\\s+(.*\\S)\\s*", std::regex::icase | std::regex::optimize);
static const std::regex extensionsHeader("\\s*Extensions\\s*", std::regex::icase | std::regex::optimize);
static const std::regex searchHeader("\\s*Search\\s*", std::regex::icase | std::regex::optimize);
static const std::regex dataLine("\\s*(\\S+)\\s+(.*\\S)\\s*", std::regex::optimize);
static const std::regex integerValue("[+-]?\\d{1,10}", std::regex::optimize);

static std::basic_string<TCHAR> filePath;

void ColumnsPlusPlusData::loadConfiguration() {

    TCHAR pluginsConfigDirectory[MAX_PATH];

    SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)pluginsConfigDirectory);
    if (PathFileExists(pluginsConfigDirectory) == FALSE)
        if (!CreateDirectory(pluginsConfigDirectory, NULL)) return;
    filePath = pluginsConfigDirectory;
    filePath += TEXT("\\ColumnsPlusPlus.data");

    std::ifstream file(filePath);
    if (!file) return;

    std::string line;
    std::getline(file, line);
    if (!file) return;

    std::smatch match;
    if (!std::regex_match(line, match, configurationHeader)) return;
//  int configLevel  = std::stoi(match[1]);  // not presently used
    int configCompat = std::stoi(match[2]);
    if (configCompat > 1) return;

    enum {sectionNone, sectionLastSettings, sectionSearch, sectionProfile, sectionExtensions} readingSection = sectionNone;
    std::wstring profileName;

    while (file) {
        std::getline(file, line);
        if      (std::regex_match(line, match, lastSettingsHeader)) readingSection = sectionLastSettings;
        else if (std::regex_match(line, match, searchHeader      )) readingSection = sectionSearch;
        else if (std::regex_match(line, match, extensionsHeader  )) readingSection = sectionExtensions;
        else if (std::regex_match(line, match, profileHeader     )) {
            readingSection = sectionProfile;
            profileName = toWide(match[1], CP_UTF8);
        }
        else if (std::regex_match(line, match, dataLine)) {
            if (readingSection == sectionLastSettings) {
                std::string setting = match[1];
                std::string value = match[2];
                strlwr(setting.data());
                if      (setting == "name"                      ) settings.profileName             = toWide(value, CP_UTF8);
                else if (setting == "elasticenabled"            ) settings.elasticEnabled          = value != "0";
                else if (setting == "decimalseparatoriscomma"   ) settings.decimalSeparatorIsComma = value != "0";
                else if (setting == "leadingtabsindent"         ) settings.leadingTabsIndent       = value != "0";
                else if (setting == "lineupall"                 ) settings.lineUpAll               = value != "0";
                else if (setting == "treateolastab"             ) settings.treatEolAsTab           = value != "0";
                else if (setting == "overridetabsize"           ) settings.overrideTabSize         = value != "0";
                else if (setting == "showonmenubar"             ) showOnMenuBar                    = value != "0";
                else if (setting == "extendsingleline"          ) extendSingleLine                 = value != "0";
                else if (setting == "extendfulllines"           ) extendFullLines                  = value != "0";
                else if (setting == "extendzerowidth"           ) extendZeroWidth                  = value != "0";
                else if (std::regex_match(value, integerValue)) {
                    if      (setting == "minimumorleadingtabsize"   ) settings.minimumOrLeadingTabSize    = std::stoi(value);
                    else if (setting == "minimumspacebetweencolumns") settings.minimumSpaceBetweenColumns = std::stoi(value);
                    else if (setting == "disableoversize"           ) disableOverSize                     = std::stoi(value);
                    else if (setting == "disableoverlines"          ) disableOverLines                    = std::stoi(value);
                }
            }
            else if (readingSection == sectionSearch) {
                std::string setting = match[1];
                std::string value = match[2];
                strlwr(setting.data());
                if      (setting == "backward" ) searchData.backward  = value != "0";
                else if (setting == "wholeword") searchData.wholeWord = value != "0";
                else if (setting == "matchcase") searchData.matchCase = value != "0";
                else if (setting == "find") {
                    if (value.length() > 2 && value.front() == '\\' && value.back() == '\\') {
                        std::wstring s = toWide(value.substr(1, value.length() - 2), CP_UTF8);
                        for (size_t i = 0; i < s.length() - 1; ++i) if (s[i] == L'\\') switch (s[i+1]) {
                            case L'r' : s.replace(i, 2, L"\r"); break;
                            case L'n' : s.replace(i, 2, L"\n"); break;
                            case L'\\': s.replace(i, 2, L"\\"); break;
                            default:;
                        }
                        searchData.findHistory.push_back(s);
                    }
                }
                else if (setting == "replace") {
                    if (value.length() > 2 && value.front() == '\\' && value.back() == '\\') {
                        std::wstring s = toWide(value.substr(1, value.length() - 2), CP_UTF8);
                        for (size_t i = 0; i < s.length() - 1; ++i) if (s[i] == L'\\') switch (s[i+1]) {
                            case L'r' : s.replace(i, 2, L"\r"); break;
                            case L'n' : s.replace(i, 2, L"\n"); break;
                            case L'\\': s.replace(i, 2, L"\\"); break;
                            default:;
                        }
                        searchData.replaceHistory.push_back(s);
                    }
                }
                else if (std::regex_match(value, integerValue)) {
                    if (setting == "mode") {
                        int i = std::stoi(value);
                        searchData.mode = i == SearchSettings::Extended ? SearchSettings::Extended
                                        : i == SearchSettings::Regex    ? SearchSettings::Regex
                                                                        : SearchSettings::Normal;
                    }
                }
            }
            else if (readingSection == sectionProfile) {
                std::string setting = match[1];
                std::string value = match[2];
                strlwr(setting.data());
                if      (setting == "leadingtabsindent"         ) profiles[profileName].leadingTabsIndent       = value != "0";
                else if (setting == "lineupall"                 ) profiles[profileName].lineUpAll               = value != "0";
                else if (setting == "treateolastab"             ) profiles[profileName].treatEolAsTab           = value != "0";
                else if (setting == "overridetabsize"           ) profiles[profileName].overrideTabSize         = value != "0";
                else if (std::regex_match(value, integerValue)) {
                    if      (setting == "minimumorleadingtabsize"   ) profiles[profileName].minimumOrLeadingTabSize    = std::stoi(value);
                    else if (setting == "minimumspacebetweencolumns") profiles[profileName].minimumSpaceBetweenColumns = std::stoi(value);
                }
            }
            else if (readingSection == sectionExtensions) {
                std::wstring extension = toWide(match[1], CP_UTF8);
                std::wstring profile   = toWide(match[2], CP_UTF8);
                if      (extension == L"new")     extensionToProfile[L"" ] = profile;
                else if (extension == L"none")    extensionToProfile[L"."] = profile;
                else if (extension == L"default") extensionToProfile[L"*"] = profile;
                else if (extension.length() > 1 && extension[0] == L'.') {
                    extension = extension.substr(1);
                    std::replace(extension.begin(), extension.end(), L'.', L' ');
                    extensionToProfile[extension] = profile == L"(disable)" ? L"" : profile;
                }
            }
        }
    }

}


void ColumnsPlusPlusData::saveConfiguration() {

    std::ofstream file(filePath);
    if (!file) return;

    file << "\xEF\xBB\xBF" << "Notepad++ Columns++ Configuration 1 1" << std::endl;

    file << std::endl << "LastSettings" << std::endl << std::endl;

    file << "elasticEnabled\t"              << settings.elasticEnabled                 << std::endl;
    file << "decimalSeparatorIsComma\t"     << settings.decimalSeparatorIsComma        << std::endl;
    file << "name\t"                        << fromWide(settings.profileName, CP_UTF8) << std::endl;
    file << "minimumOrLeadingTabSize\t"     << settings.minimumOrLeadingTabSize        << std::endl;
    file << "minimumSpaceBetweenColumns\t"  << settings.minimumSpaceBetweenColumns     << std::endl;
    file << "leadingTabsIndent\t"           << settings.leadingTabsIndent              << std::endl;
    file << "lineUpAll\t"                   << settings.lineUpAll                      << std::endl;
    file << "treatEolAsTab\t"               << settings.treatEolAsTab                  << std::endl;
    file << "overrideTabSize\t"             << settings.overrideTabSize                << std::endl;
    file << "disableOverSize\t"             << disableOverSize                         << std::endl;
    file << "disableOverLines\t"            << disableOverLines                        << std::endl;
    file << "showOnMenuBar\t"               << showOnMenuBar                           << std::endl;
    file << "extendSingleLine\t"            << extendSingleLine                        << std::endl;
    file << "extendFullLines\t"             << extendFullLines                         << std::endl;
    file << "extendZeroWidth\t"             << extendZeroWidth                         << std::endl;

    file << std::endl << "Search" << std::endl << std::endl;

    file << "mode\t"      << searchData.mode      << std::endl;
    file << "backward\t"  << searchData.backward  << std::endl;
    file << "wholeWord\t" << searchData.wholeWord << std::endl;
    file << "matchCase\t" << searchData.matchCase << std::endl;

    for (auto it = searchData.findHistory.size() > 16 ? searchData.findHistory.end() - 16 : searchData.findHistory.begin();
        it != searchData.findHistory.end(); ++it) {
        std::wstring s = *it;
        for (size_t i = 0; i < s.length(); ++i) switch (s[i]) {
            case L'\r': s.replace(i, 1, L"\\r" ); ++i; break;
            case L'\n': s.replace(i, 1, L"\\n" ); ++i; break;
            case L'\\': s.replace(i, 1, L"\\\\"); ++i; break;
            default:;
            }
        file << "find\t\\" << fromWide(s, CP_UTF8) << "\\" << std::endl;
    }

    for (auto it = searchData.replaceHistory.size() > 16 ? searchData.replaceHistory.end() - 16 : searchData.replaceHistory.begin();
        it != searchData.replaceHistory.end(); ++it) {
        std::wstring s = *it;
        for (size_t i = 0; i < s.length(); ++i) switch (s[i]) {
            case L'\r': s.replace(i, 1, L"\\r" ); ++i; break;
            case L'\n': s.replace(i, 1, L"\\n" ); ++i; break;
            case L'\\': s.replace(i, 1, L"\\\\"); ++i; break;
            default:;
            }
        file << "replace\t\\" << fromWide(s, CP_UTF8) << "\\" << std::endl;
    }

    for (const auto& p : profiles) if (p.first != L"Classic" && p.first != L"General" && p.first != L"Tabular") {
        file << std::endl << "Profile\t" << fromWide(p.first, CP_UTF8) << std::endl << std::endl;
        file << "minimumOrLeadingTabSize\t"     << settings.minimumOrLeadingTabSize        << std::endl;
        file << "minimumSpaceBetweenColumns\t"  << settings.minimumSpaceBetweenColumns     << std::endl;
        file << "leadingTabsIndent\t"           << settings.leadingTabsIndent              << std::endl;
        file << "lineUpAll\t"                   << settings.lineUpAll                      << std::endl;
        file << "treatEolAsTab\t"               << settings.treatEolAsTab                  << std::endl;
        file << "overrideTabSize\t"             << settings.overrideTabSize                << std::endl;
    }

    if (extensionToProfile.size()) {
        file << std::endl << "Extensions" << std::endl << std::endl;
        for (const auto& xtp : extensionToProfile)
            if      (xtp.first == L"" ) file << "new\t"     << fromWide(xtp.second, CP_UTF8) << std::endl;
            else if (xtp.first == L".") file << "none\t"    << fromWide(xtp.second, CP_UTF8) << std::endl;
            else if (xtp.first == L"*") file << "default\t" << fromWide(xtp.second, CP_UTF8) << std::endl;
            else  {
                std::string extension = fromWide(xtp.first, CP_UTF8);
                std::replace(extension.begin(), extension.end(), ' ', '.');
                file << "." << extension << "\t" << (xtp.second.length() ? fromWide(xtp.second, CP_UTF8) : "(disable)") << std::endl;
            }
    }

}


void ColumnsPlusPlusData::initializeBuiltinElasticTabstopsProfiles() {

    ElasticTabsProfile& classic = profiles[L"Classic"];
    classic.leadingTabsIndent          = false;
    classic.lineUpAll                  = false;
    classic.treatEolAsTab              = false;
    classic.overrideTabSize            = false;
    classic.minimumOrLeadingTabSize    = 4;
    classic.minimumSpaceBetweenColumns = 2;

    ElasticTabsProfile& general = profiles[L"General"];
    general.leadingTabsIndent          = true;
    general.lineUpAll                  = false;
    general.treatEolAsTab              = false;
    general.overrideTabSize            = false;
    general.minimumOrLeadingTabSize    = 4;
    general.minimumSpaceBetweenColumns = 2;

    ElasticTabsProfile& tabular = profiles[L"Tabular"];
    tabular.leadingTabsIndent          = false;
    tabular.lineUpAll                  = true;
    tabular.treatEolAsTab              = true;
    tabular.overrideTabSize            = true;
    tabular.minimumOrLeadingTabSize    = 1;
    tabular.minimumSpaceBetweenColumns = 2;

}