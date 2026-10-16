#include "engine/gge_catalog_snapshot.h"
#include <cmath>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace os::gge {
namespace {
constexpr size_t kMaxBytes=16*1024*1024,kMaxRows=4096,kMaxText=4096;
std::string trim(std::string s) { const auto a=s.find_first_not_of(" \t\r"),b=s.find_last_not_of(" \t\r"); return a==std::string::npos?std::string():s.substr(a,b-a+1); }
bool u32v(const std::string& s,u32& out) {
    if(s.empty()||s.size()>10||s[0]=='-')return false;
    errno=0; char* end=nullptr; const unsigned long value=std::strtoul(s.c_str(),&end,0);
    if(errno==ERANGE||end==s.c_str()||!end||*end!='\0'||value>0xfffffffful)return false;
    out=(u32)value; return true;
}
bool floatv(const std::string& s,float& out) { char* e=nullptr; out=std::strtof(s.c_str(),&e); return e!=s.c_str()&&*e==0&&std::isfinite(out); }
bool hexbytes(const std::string& s,std::string& out) { if(s.size()>kMaxText*8||(s.size()&1))return false; out.clear();out.reserve(s.size()/2);for(size_t i=0;i<s.size();i+=2){auto n=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};int a=n(s[i]),b=n(s[i+1]);if(a<0||b<0)return false;out.push_back(char((a<<4)|b));}return true; }
bool utf8u16(const std::string& s,std::u16string& out) { out.clear(); for(size_t i=0;i<s.size();){u8 c=(u8)s[i++];u32 cp=0;int n=0;if(c<0x80)cp=c;else if((c&0xe0)==0xc0){cp=c&31;n=1;}else if((c&0xf0)==0xe0){cp=c&15;n=2;}else if((c&0xf8)==0xf0){cp=c&7;n=3;}else return false;if(i+n>s.size())return false;for(int j=0;j<n;++j){u8 q=(u8)s[i++];if((q&0xc0)!=0x80)return false;cp=(cp<<6)|(q&63);}if((cp>0x10ffff)||(cp>=0xd800&&cp<=0xdfff)||(cp<0x80&&n)|| (cp<0x800&&n>1)||(cp<0x10000&&n>2))return false;if(cp<=0xffff)out.push_back((char16_t)cp);else{cp-=0x10000;out.push_back((char16_t)(0xd800+(cp>>10)));out.push_back((char16_t)(0xdc00+(cp&1023)));}if(out.size()>kMaxText)return false;}return true; }
bool status(const std::string& s,CatalogAvailability& out) { if(s=="Available"){out=CatalogAvailability::Available;return true;}if(s=="Unknown"){out=CatalogAvailability::Unknown;return true;}return false; }
bool row(const std::vector<std::string>& t,bool saved,SavedGameMetadata* sp,StarMetadata* cp,std::string& e) {
    if(t.size()!=static_cast<size_t>(saved?13:9)) {e="catalog snapshot row field count";return false;} size_t n=0;u32 id=0,stage=0,flags=0,probe=0,h1=0,h2=0,terrain=0,terrain_image=0;float x=0,y=0,z=0;
    if(!u32v(t[n++],id)||!floatv(t[n++],x)||!floatv(t[n++],y)||!floatv(t[n++],z)||!u32v(t[n++],stage)||!u32v(t[n++],flags)) {e="catalog snapshot numeric row field";return false;}
    std::string name,stem; if(!hexbytes(t[n++],name)||!utf8u16(name,saved?sp->star.name:cp->name)) {e="catalog snapshot name UTF8/hex invalid";return false;}
    if(saved) { if(!hexbytes(t[n++],stem)||!utf8u16(stem,sp->save_stem)){e="catalog snapshot save stem invalid";return false;} if(!u32v(t[n++],probe)||!u32v(t[n++],h1)||!u32v(t[n++],h2)||!u32v(t[n++],terrain)||!u32v(t[n++],terrain_image)){e="catalog snapshot saved fields invalid";return false;} }
    else if(!u32v(t[n++],terrain)||!u32v(t[n++],terrain_image)) {e="catalog snapshot candidate terrain fields invalid";return false;}
    if(terrain>1||(terrain&&terrain_image==0)){e="catalog snapshot terrain observation invalid (loaded requires image instance)";return false;}
    if(saved){sp->star.star_id=id;sp->star.position={x,y,z};sp->stage=stage;sp->source_flags=flags;sp->probe_result=probe!=0;sp->header={h1,h2};sp->star.terrain_loaded=terrain!=0;sp->star.terrain_image=terrain_image;}
    else{cp->star_id=id;cp->position={x,y,z};cp->terrain_loaded=terrain!=0;cp->terrain_image=terrain_image;}
    return true;
}
}
bool load_catalog_snapshot(const std::string& path,CatalogInput& out,std::string& error) {
    if(path.empty()||path.find('\0')!=std::string::npos){error="catalog snapshot path empty/NUL";return false;}
    std::ifstream f(path,std::ios::binary);if(!f){error="catalog snapshot cannot open explicitly selected file";return false;}
    f.seekg(0,std::ios::end);auto size=f.tellg();if(size<0||static_cast<unsigned long long>(size)>kMaxBytes){error="catalog snapshot exceeds 16MiB host bound";return false;}f.seekg(0);std::string body((size_t)size,'\0');if(!body.empty()&&!f.read(body.data(),(std::streamsize)body.size())){error="catalog snapshot short read";return false;}
    std::istringstream in(body);std::string line,version;CatalogInput result;size_t saved_count=0,candidate_count=0,saved_rows=0,candidate_rows=0;bool saw_saved=false,saw_candidate=false,saw_saved_count=false,saw_candidate_count=false,end=false;
    if(!std::getline(in,line)){error="catalog snapshot version header missing";return false;}
    if(line.size()>=3&&(u8)line[0]==0xef&&(u8)line[1]==0xbb&&(u8)line[2]==0xbf)line.erase(0,3);
    if(trim(line)!="GGE_CATALOG_V1"){error="catalog snapshot version header missing";return false;}
    while(std::getline(in,line)){line=trim(line);if(line.empty()||line[0]=='#')continue;std::vector<std::string> t;std::istringstream ls(line);std::string x;while(std::getline(ls,x,'\t'))t.push_back(x);
        if(t.size()==2&&t[0]=="saved_status"){if(saw_saved||!status(t[1],result.saved_status)){error="catalog snapshot saved_status invalid/duplicate";return false;}saw_saved=true;continue;}
        if(t.size()==2&&t[0]=="candidate_status"){if(saw_candidate||!status(t[1],result.candidate_status)){error="catalog snapshot candidate_status invalid/duplicate";return false;}saw_candidate=true;continue;}
        if(t.size()==2&&t[0]=="saved_count"){u32 v=0;if(saw_saved_count||!u32v(t[1],v)||v>kMaxRows){error="catalog snapshot saved_count invalid/duplicate";return false;}saved_count=v;saw_saved_count=true;continue;}
        if(t.size()==2&&t[0]=="candidate_count"){u32 v=0;if(saw_candidate_count||!u32v(t[1],v)||v>kMaxRows){error="catalog snapshot candidate_count invalid/duplicate";return false;}candidate_count=v;saw_candidate_count=true;continue;}
        if(t[0]=="saved"){SavedGameMetadata rowv;if(!row({t.begin()+1,t.end()},true,&rowv,nullptr,error))return false;result.saved.push_back(std::move(rowv));++saved_rows;continue;}
        if(t[0]=="candidate"){StarMetadata rowv;if(!row({t.begin()+1,t.end()},false,nullptr,&rowv,error))return false;result.candidates.push_back(std::move(rowv));++candidate_rows;continue;}
        if(t.size()==1&&t[0]=="END"){end=true;break;} error="catalog snapshot unknown line";return false;
    }
    if(!saw_saved||!saw_candidate||!saw_saved_count||!saw_candidate_count||!end||saved_rows!=saved_count||candidate_rows!=candidate_count){error="catalog snapshot incomplete/status/count mismatch";return false;}
    out=std::move(result);error.clear();return true;
}
} // namespace os::gge
