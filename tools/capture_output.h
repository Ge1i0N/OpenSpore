#pragma once
// Tool-local output policy, not reconstructed game serialization or native behavior.
#include <filesystem>
#include <cerrno>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace capture {
inline std::filesystem::path path(const std::string& utf8) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()),utf8.size()));
}
inline std::string utf8(const std::u16string& input) {
    std::string out;
    if(input.size()>32768)throw std::runtime_error("capture UTF-16 host limit");
    for(size_t i=0;i<input.size();++i) {
        unsigned cp=input[i];
        if(cp>=0xd800&&cp<=0xdbff) {
            if(i+1>=input.size()||input[i+1]<0xdc00||input[i+1]>0xdfff)throw std::runtime_error("unpaired UTF-16 surrogate");
            cp=0x10000+((cp-0xd800)<<10)+(input[++i]-0xdc00);
        }else if(cp>=0xdc00&&cp<=0xdfff)throw std::runtime_error("unpaired UTF-16 surrogate");
        if(cp<0x80)out+=char(cp);
        else if(cp<0x800){out+=char(0xc0|(cp>>6));out+=char(0x80|(cp&63));}
        else if(cp<0x10000){out+=char(0xe0|(cp>>12));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
        else {out+=char(0xf0|(cp>>18));out+=char(0x80|((cp>>12)&63));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}
    }
    return out;
}
inline std::string text(const std::string& input) {
    std::ostringstream out;out<<'"'<<std::hex<<std::setfill('0');
    for(unsigned char c:input) {
        if(c=='"'||c=='\\')out<<'\\'<<char(c);
        else if(c<32)out<<"\\u"<<std::setw(4)<<unsigned(c);
        else out<<char(c);
    }
    out<<'"';return out.str();
}
inline void publish_new(const std::filesystem::path& dest,const std::string& body) {
    if(body.size()>16*1024*1024)throw std::runtime_error("capture output exceeds 16 MiB host bound");
    // Write a NEW sibling completely, then publish without replacement. An interruption leaves at
    // worst a private partial sibling, never a partial final file. No broad cleanup or save writes.
    std::filesystem::path temp;
#ifdef _WIN32
    HANDLE file=INVALID_HANDLE_VALUE;
    for(unsigned i=0;i<128;++i) {
        temp=dest;temp+=L".partial-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(i);
        file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE)break;
        if(GetLastError()!=ERROR_FILE_EXISTS&&GetLastError()!=ERROR_ALREADY_EXISTS)throw std::runtime_error("cannot create NEW output sibling");
    }
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("no free NEW output sibling");
    DWORD written=0;
    const bool wrote=WriteFile(file,body.data(),static_cast<DWORD>(body.size()),&written,nullptr)&&written==body.size()&&FlushFileBuffers(file);
    const bool closed=CloseHandle(file)!=0;
    if(!wrote||!closed){DeleteFileW(temp.c_str());throw std::runtime_error("capture output write/flush/close failed");}
    // No MOVEFILE_REPLACE_EXISTING: this also refuses a final path created by a racing writer.
    if(!MoveFileExW(temp.c_str(),dest.c_str(),MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str());throw std::runtime_error("NEW capture publication refused; destination unchanged");
    }
#else
    int file=-1;
    for(unsigned i=0;i<128;++i) {
        temp=dest;temp+=".partial-"+std::to_string(getpid())+"-"+std::to_string(i);
        file=::open(temp.c_str(),O_CREAT|O_EXCL|O_WRONLY,0600);
        if(file>=0)break;
        if(errno!=EEXIST)throw std::runtime_error("cannot create NEW output sibling");
    }
    if(file<0)throw std::runtime_error("no free NEW output sibling");
    size_t at=0;
    while(at<body.size()){const auto n=::write(file,body.data()+at,body.size()-at);if(n<=0)break;at+=static_cast<size_t>(n);}
    const bool flushed=::fsync(file)==0,closed=::close(file)==0;
    if(at!=body.size()||!flushed||!closed){::unlink(temp.c_str());throw std::runtime_error("capture write/flush/close failed");}
    const bool linked=::link(temp.c_str(),dest.c_str())==0;::unlink(temp.c_str());
    if(!linked)throw std::runtime_error("NEW capture publication refused; destination unchanged");
#endif
}
} // namespace capture
