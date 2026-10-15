// Direct original-package streaming + explicitly named NEW controlled file fixtures, never saves.
#include "engine/gge_image_provider.h"
#include "render/imgfile.h"
#include <Windows.h>
#include <winioctl.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <type_traits>
using namespace os;
using namespace os::gge;
using Bytes=std::vector<u8>;
namespace fs=std::filesystem;
static_assert(!std::is_copy_constructible_v<StreamingImageProvider>);
static void le(Bytes& b,size_t at,u32 v) { for (size_t i=0;i<4;++i) b[at+i]=(u8)(v>>(8*i)); }
static u32 pop(u32 n) { u32 count=0; for (;n;n>>=1) count+=n&1; return count; }
struct Fixture { Bytes bytes; size_t index=0,record=0,offset=0,size=0,mem=0,flags=0; };
static Fixture fixture(const ImageKey& key,const Bytes& data,u32 mem,u16 compression=0,u32 kind=4,bool duplicate=false) {
    Fixture f; f.index=0x60+data.size(); const u32 count=duplicate?2:1,prefix=4+4*pop(kind),stride=32-4*pop(kind);
    f.bytes.resize(f.index+prefix+count*stride,0);
    le(f.bytes,0,0x46504244); le(f.bytes,4,3); le(f.bytes,0x24,count); le(f.bytes,0x2c,prefix+count*stride);
    le(f.bytes,0x3c,3); le(f.bytes,0x40,(u32)f.index); std::copy(data.begin(),data.end(),f.bytes.begin()+0x60);
    le(f.bytes,f.index,kind); size_t at=f.index+4;
    for (u32 bit=0;bit<3;++bit) if (kind&(1u<<bit)) { le(f.bytes,at,bit==0?key.type:bit==1?key.group:0); at+=4; }
    f.record=at;
    for (u32 i=0;i<count;++i) {
        for (u32 bit=0;bit<3;++bit) if (!(kind&(1u<<bit))) { le(f.bytes,at,bit==0?key.type:bit==1?key.group:0); at+=4; }
        le(f.bytes,at,key.instance); at+=4;
        f.offset=at; le(f.bytes,at,0x60); at+=4;
        f.size=at; le(f.bytes,at,(u32)data.size()); at+=4;
        f.mem=at; le(f.bytes,at,mem); at+=4;
        f.flags=at; le(f.bytes,at,0x10000u|compression); at+=4;
    }
    return f;
}
static Bytes literals(const Bytes& bytes) {
    Bytes b{0x10,0xfb,u8(bytes.size()>>16),u8(bytes.size()>>8),u8(bytes.size())}; size_t at=0;
    while (bytes.size()-at>=4) {
        const size_t n=std::min<size_t>(112,(bytes.size()-at)&~size_t(3));
        b.push_back(u8(0xe0+(n-4)/4)); b.insert(b.end(),bytes.begin()+at,bytes.begin()+at+n); at+=n;
    }
    b.push_back(u8(0xfc+bytes.size()-at)); b.insert(b.end(),bytes.begin()+at,bytes.end()); return b;
}
struct TempFiles {
    fs::path dir; std::vector<fs::path> files; unsigned next=0;
    ~TempFiles() { std::error_code ec; for (const auto& f:files) fs::remove(f,ec); fs::remove(dir,ec); }
    fs::path path() { return dir/("controlled-"+std::to_string(next++)+".package"); }
    bool write(const fs::path& p,const Bytes& bytes) {
        HANDLE h=CreateFileW(p.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (h==INVALID_HANDLE_VALUE) return false;
        files.push_back(p); DWORD n=0;
        const bool ok=WriteFile(h,bytes.data(),(DWORD)bytes.size(),&n,nullptr)&&n==bytes.size(); CloseHandle(h); return ok;
    }
    bool sparse(const fs::path& p,const ImageKey& key,const Bytes& png,u32 data_offset,u32 packed_size=0,u64 file_end=0) {
        HANDLE h=CreateFileW(p.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (h==INVALID_HANDLE_VALUE) return false;
        files.push_back(p); DWORD n=0;
        if (!DeviceIoControl(h,FSCTL_SET_SPARSE,nullptr,0,nullptr,0,&n,nullptr)) { CloseHandle(h); return false; }
        const auto model=fixture(key,png,(u32)png.size());
        Bytes header(model.bytes.begin(),model.bytes.begin()+0x60),index(model.bytes.begin()+model.index,model.bytes.end());
        const u32 span=packed_size?packed_size:(u32)png.size(),index_offset=data_offset+span;
        le(header,0x40,index_offset); le(index,model.offset-model.index,data_offset);
        if (packed_size) le(index,model.size-model.index,packed_size);
        const auto write_at=[&](u64 offset,const Bytes& bytes) {
            LARGE_INTEGER at; at.QuadPart=(LONGLONG)offset; DWORD count=0;
            return SetFilePointerEx(h,at,nullptr,FILE_BEGIN)&&WriteFile(h,bytes.data(),(DWORD)bytes.size(),&count,nullptr)&&count==bytes.size();
        };
        bool ok=write_at(0,header)&&write_at(data_offset,png)&&write_at(index_offset,index);
        if (ok&&file_end) { LARGE_INTEGER end; end.QuadPart=(LONGLONG)file_end; ok=SetFilePointerEx(h,end,nullptr,FILE_BEGIN)&&SetEndOfFile(h); }
        CloseHandle(h); return ok;
    }
};
int main(int argc,char** argv) {
    if (argc!=3) { std::cerr<<"usage: gge_streaming_image_verify <explicit-original-Graphics.package> <NEW-test-directory>\n"; return 2; }
    std::error_code ec; const fs::path dir=argv[2];
    if (fs::exists(dir,ec)||!fs::create_directory(dir,ec)) { std::cerr<<"test directory must be NEW; no existing files touched\n"; return 2; }
    TempFiles temp; temp.dir=dir; size_t checks=0; std::string error;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    const ImageKey key{0xb89d2cd2u,0x2f7d0004u,0x31a44893u};
    StreamingImageProvider stream;
    if (!check(stream.read(key).status==ImageReadStatus::Unknown,"closed provider Unknown")) return 1;
    if (!check(stream.open(argv[1],error)&&stream.package().header().file_size>256*1024*1024,"direct large ORIGINAL package opens without full-file vector")) return 1;
    const auto matches=stream.package().find_records({key.instance,key.type,key.group}); auto real=stream.read(key);
    if (!check(matches.size()==1&&real.status==ImageReadStatus::Found&&real.image.pixels&&real.image.pixels->width==256&&real.image.pixels->height==512&&
        real.image.pixels->provenance.find(argv[1])!=std::string::npos&&real.image.pixels->provenance.find("record["+std::to_string(matches[0])+"]")!=std::string::npos,
        "actual star atlas decoded directly with original file/record provenance")) return 1;
    pkg::Package independent; Bytes png,pixels; int width=0,height=0;
    if (!check(independent.open(argv[1])&&independent.read_resource_decoded(matches[0],png)&&
        image::decode_from_memory(png.data(),png.size(),pixels,width,height)&&pixels==real.image.pixels->bgra,
        "direct provider BGRA equals independent actual-resource decode")) return 1;
    ExplicitImageProvider small;
    if (!check(!small.open(argv[1],error),"old 256MiB memory provider limit unchanged")) return 1;
    if (!check(stream.read({key.instance,key.type,key.group^0x80000000u}).status==ImageReadStatus::Missing,
        "exact group mismatch Missing, no other-package fallback")) return 1;
    if (!check(stream.read({0,key.type,key.group}).status==ImageReadStatus::Unsupported&&
        stream.read({key.instance,42,key.group}).status==ImageReadStatus::Unsupported,"unsupported zero/non-PNG keys")) return 1;
    const auto retained=real.image.pixels; stream.close();
    if (!check(stream.read(key).status==ImageReadStatus::Unknown&&retained->bgra==pixels,"decoded immutable pixels outlive closed file provider")) return 1;
    const auto packed=literals(png);
    for (u32 mask=0;mask<8;++mask) for (bool compressed:{false,true}) {
        const auto f=fixture(key,compressed?packed:png,(u32)png.size(),compressed?0xffff:0,mask);
        const auto path=temp.path(); if (!check(temp.write(path,f.bytes),"new controlled fixture write")) return 1;
        if (!check(stream.open(path.string(),error)&&stream.package().kind()==mask,"all three shared bits accounted, bounded directory parsed")) return 1;
        auto result=stream.read(key);
        if (!check(result.status==ImageReadStatus::Found&&result.image.pixels->bgra==pixels,"raw/RefPack same PNG decodes identically for each shared mask")) return 1;
    }
    auto f=fixture(key,png,(u32)png.size(),0,4,true); auto path=temp.path();
    if (!check(temp.write(path,f.bytes)&&stream.open(path.string(),error)&&stream.read(key).status==ImageReadStatus::Ambiguous,
        "duplicate exact keys retained, not first-record chosen")) return 1;
    const auto rejected=[&](Bytes bytes) {
        const auto p=temp.path();
        return temp.write(p,bytes)&&!stream.open(p.string(),error)&&!error.empty()&&stream.read(key).status==ImageReadStatus::Unknown;
    };
    f=fixture(key,png,(u32)png.size());
    for (const auto& mutation:std::vector<std::pair<size_t,u32>>{{0,0},{4,2},{8,1},{0x3c,1},{0x40,0x10},{0x24,65537},{0x24,2},{f.index,8},{0x2c,2},{0x2c,0xffffffffu}}) {
        auto bytes=f.bytes; le(bytes,mutation.first,mutation.second);
        if (!check(rejected(std::move(bytes)),"unsupported/corrupt directory rejected before resource reads, no stale Ready")) return 1;
    }
    if (!check(rejected(Bytes(32,0)),"short header rejected")) return 1;
    auto short_index=f.bytes; short_index.resize(f.index+2);
    if (!check(rejected(std::move(short_index)),"truncated index rejected")) return 1;
    const auto bad_resource=[&](Bytes bytes,ImageReadStatus expected) {
        const auto p=temp.path();
        if (!temp.write(p,bytes)||!stream.open(p.string(),error)) return false;
        auto result=stream.read(key); return result.status==expected&&!result.image.pixels&&!result.error.empty();
    };
    for (const auto& mutation:std::vector<std::pair<size_t,u32>>{{f.flags,0x10001},{f.flags,0},{f.index+4,1},{f.mem,64*1024*1024+1}}) {
        auto bytes=f.bytes; le(bytes,mutation.first,mutation.second);
        if (!check(bad_resource(std::move(bytes),ImageReadStatus::Unsupported),"flags/high-key-word/compression/resource budget Unsupported")) return 1;
    }
    for (const auto& mutation:std::vector<std::pair<size_t,u32>>{{f.mem,0},{f.mem,(u32)png.size()+1},{f.offset,0},{f.offset,(u32)f.index+1},{f.size,0}}) {
        auto bytes=f.bytes; le(bytes,mutation.first,mutation.second);
        if (!check(bad_resource(std::move(bytes),ImageReadStatus::ReadFailure),"malformed resource span/size is failed read, not Missing")) return 1;
    }
    auto corrupt=png; corrupt[0]=0;
    if (!check(bad_resource(fixture(key,corrupt,(u32)corrupt.size()).bytes,ImageReadStatus::ReadFailure),"bad PNG header rejected")) return 1;
    corrupt=png; corrupt[16]=0x7f;
    if (!check(bad_resource(fixture(key,corrupt,(u32)corrupt.size()).bytes,ImageReadStatus::Unsupported),"PNG IHDR dimension bound before decode")) return 1;
    corrupt.resize(33); std::copy(png.begin(),png.begin()+33,corrupt.begin());
    if (!check(bad_resource(fixture(key,corrupt,33).bytes,ImageReadStatus::ReadFailure),"truncated PNG payload rejected")) return 1;
    corrupt=packed; corrupt[2]^=1;
    if (!check(bad_resource(fixture(key,corrupt,(u32)png.size(),0xffff).bytes,ImageReadStatus::ReadFailure),"RefPack header/index size mismatch rejects before allocation")) return 1;
    path=temp.path();
    if (!check(temp.sparse(path,key,png,0x80000100u)&&stream.open(path.string(),error)&&stream.package().header().file_size>0x80000000ull&&
        stream.read(key).status==ImageReadStatus::Found,"SPARSE controlled package: data AND directory above 2GiB use 64-bit seeks")) return 1;
    path=temp.path();
    if (!check(temp.sparse(path,key,png,0x60,65*1024*1024)&&stream.open(path.string(),error)&&stream.read(key).status==ImageReadStatus::Unsupported,
        "sparse oversized packed resource rejected before payload allocation")) return 1;
    path=temp.path();
    if (!check(temp.sparse(path,key,png,0x60,0,(u64{1}<<32)+1)&&!stream.open(path.string(),error)&&stream.read(key).status==ImageReadStatus::Unknown,
        "provider 4GiB file bound rejects sparse oversized file before index allocation")) return 1;
    pkg::Package bounded;
    if (!check(!bounded.open_bounded(argv[1],{1024,65536,4*1024*1024})&&!bounded.ok()&&bounded.records().empty(),"generic bounded open file limit before directory allocation")) return 1;
    bounded.close();
    if (!check(!bounded.open_bounded(argv[1],{u64{1}<<32,1,4*1024*1024})&&!bounded.ok()&&bounded.records().empty(),"record allocation limit on same handle")) return 1;
    bounded.close();
    if (!check(!bounded.open_bounded(argv[1],{u64{1}<<32,65536,4})&&!bounded.ok()&&bounded.records().empty(),"index allocation limit on same handle")) return 1;
    if (!check(!stream.open((dir/"does-not-exist.package").string(),error)&&stream.read(key).status==ImageReadStatus::Unknown,"failed reopen retires previous provider")) return 1;
    if (!check(!stream.open(std::string("x\0y",3),error)&&!stream.open("",error),"empty/NUL path not silently truncated")) return 1;
    path=temp.path();
    if (!check(temp.write(path,f.bytes)&&stream.open(path.string(),error),"controlled selected-source sharing fixture opens")) return 1;
    fs::resize_file(path,0x60,ec);
    const auto after_truncate=stream.read(key);
    if (!check(ec.value()==ERROR_SHARING_VIOLATION&&after_truncate.status==ImageReadStatus::Found,
        "Windows CRT read-handle sharing denies concurrent truncation; unchanged original fixture remains readable")) return 1;
    stream.close();
    path=temp.path();
    if (!check(temp.write(path,f.bytes),"lifetime fixture")) return 1;
    const auto broken_path=temp.path();
    if (!check(temp.write(broken_path,Bytes(32,0)),"failed-open lifetime fixture")) return 1;
    bool cycles=true;
    for (unsigned i=0;i<544&&cycles;++i) {
        cycles=stream.open(path.string(),error)&&stream.read(key).status==ImageReadStatus::Found&&
            !stream.open(broken_path.string(),error)&&stream.read(key).status==ImageReadStatus::Unknown;
        stream.close();
    }
    if (!check(cycles,"544 owned open/read/failed-reopen/close cycles exceed CRT handle limit without leaking")) return 1;
    stream.close(); bounded.close(); independent.close();
    std::cout<<checks<<" streaming provider/bounds/key/error/lifetime checks PASS; original atlas record="<<matches[0]
        <<"; sparse >2GiB data/index PASS; no profile search or scene readiness\n";
    return 0;
}
