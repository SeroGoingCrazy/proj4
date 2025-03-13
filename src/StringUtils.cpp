#include "StringUtils.h"
#include <string>
#include <cctype>
#include <algorithm>

namespace StringUtils{

std::string Slice(const std::string &str, ssize_t start, ssize_t end) noexcept {
    ssize_t len = static_cast<ssize_t>(str.size());

    if (start < 0) {
        start += len;
    }

    if (end <= 0) {
        end += len;
    }

    if (start < 0) {
        start = 0;
    }

    if (end > len) {
        end = len;
    }

    if (start >= end) {
        return "";
    }

    return str.substr(start, end - start);
}



std::string Capitalize(const std::string &str) noexcept {
    if (str.empty()) {
        return "";
    }
    
    std::string res;
    // Capitalize first character
    res += static_cast<char>(std::toupper(str[0]));
    
    // Lowercase the rest starting from index 1
    for (size_t i = 1; i < str.length(); ++i) {
        res += static_cast<char>(std::tolower(str[i]));
    }
    
    return res;
}

std::string Upper(const std::string &str) noexcept{
    std::string res = str;
    if (str.empty()){
        return "";
    }

    std::transform(res.begin(), res.end(), res.begin(),
                    [](unsigned char c) { return std::toupper(c); });
    return res;
}

std::string Lower(const std::string &str) noexcept{
    std::string res = str;
    if (str.empty()){
        return "";
    }

    std::transform(res.begin(), res.end(), res.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return res;
}

std::string LStrip(const std::string &str) noexcept {
    static const std::string WHITESPACE = " \t\n\r\f\v";

    size_t start = 0;
    while (start < str.length() && WHITESPACE.find(str[start]) != std::string::npos) {
        start++;
    }

    return str.substr(start);
}

std::string RStrip(const std::string &str) noexcept{
    static const std::string WHITESPACE = " \t\n\r\f\v";

    size_t end = str.length();
    while (end > 0 && WHITESPACE.find(str[end-1]) != std::string::npos) {
        end--;
    }

    return str.substr(0, end);
}

std::string Strip(const std::string &str) noexcept{
    return RStrip(LStrip(str));
}

std::string Center(const std::string &str, int width, char fill) noexcept {
    if (str.size() >= static_cast<size_t>(width)) {
        return str;
    }
    
    size_t total_pad = static_cast<size_t>(width) - str.size();
    size_t left_pad = total_pad / 2;
    size_t right_pad = total_pad - left_pad;
    
    return std::string(left_pad, fill) + str + std::string(right_pad, fill);
}

std::string LJust(const std::string &str, int width, char fill) noexcept {
    if (str.size() >= static_cast<size_t>(width)) {
        return str;
    }
    
    return str + std::string(static_cast<size_t>(width) - str.size(), fill);
}

std::string RJust(const std::string &str, int width, char fill) noexcept {
    if (str.size() >= static_cast<size_t>(width)) {
        return str;
    }
    
    return std::string(static_cast<size_t>(width) - str.size(), fill) + str;
}

std::string Replace(const std::string &str, const std::string &old, const std::string &rep) noexcept {
    if (old.empty()) {
        return str; 
    }

    std::string result = str;  // Create a copy to modify
    size_t pos = 0;
    while ((pos = result.find(old, pos)) != std::string::npos) {
        result.replace(pos, old.length(), rep);
        pos += rep.length();
    }

    return result;
}


std::vector<std::string> Split(const std::string &str, const std::string &splt) noexcept {
    std::vector<std::string> ls;
    if (splt.empty()) {
        size_t start = 0;
        while (start < str.size()) {
            while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
                ++start;
            }
            if (start >= str.size()) break;
            size_t end = start;
            while (end < str.size() && !std::isspace(static_cast<unsigned char>(str[end]))) {
                ++end;
            }
            ls.push_back(str.substr(start, end - start));
            start = end;
        }
        return ls;
    }
    size_t index = 0;
    while (true) {
        size_t pos = str.find(splt, index);
        if (pos == std::string::npos) {
            ls.push_back(str.substr(index));
            break;
        }
        ls.push_back(str.substr(index, pos - index));
        index = pos + splt.size();
    }
    return ls;
}

std::string Join(const std::string &str, const std::vector< std::string > &vect) noexcept{
    if (vect.empty()) {
        return {};
    }
    std::string res = vect[0];
    
    for (size_t i=1; i<vect.size(); ++i){
        res += str;
        res += vect[i];
    }

    return res;
}

std::string ExpandTabs(const std::string &str, int tabsize) noexcept {
    std::string result;
    int column = 0;

    for (char c : str) {
        if (c == '\n') {
            result.push_back(c);
            column = 0;
        } else if (c == '\t') {
            if (tabsize <= 0) {
                continue;
            } else {
                int spaceCount = tabsize - (column % tabsize);
                result.append(spaceCount, ' ');
                column += spaceCount;
            }
        } else {
            result.push_back(c);
            ++column;
        }
    }

    return result;
}


int EditDistance(const std::string &left, const std::string &right, bool ignorecase) noexcept {
    std::string s1 = ignorecase ? Lower(left) : left;
    std::string s2 = ignorecase ? Lower(right) : right;
    int m = static_cast<int>(s1.length());
    int n = static_cast<int>(s2.length());

    std::vector<std::vector<int>> matr(m + 1, std::vector<int>(n + 1, 0));

    for (int i = 0; i <= m; ++i) {
        matr[i][0] = i;
    }

    for (int j = 0; j <= n; ++j) {
        matr[0][j] = j;
    }

    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                matr[i][j] = matr[i - 1][j - 1];
            } else {
                matr[i][j] = 1 + std::min({matr[i - 1][j], matr[i][j - 1], matr[i - 1][j - 1]});
            }
        }
    }

    return matr[m][n];
}

};