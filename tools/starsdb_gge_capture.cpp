// Explicit structural save observations, NOT a GGE_CATALOG_V1 producer.
// No terrain query, name generation, runtime refill or writes to input files.
#include "engine/planet_record.h"
#include "engine/save_header.h"
#include "capture_output.h"
#include <charconv>
#include <cmath>
#include <iostream>
#include <map>
#include <set>

using namespace os;
using namespace os::starsdb;
using capture::text;

namespace {
std::string hex(u32 n) {
    std::ostringstream out; out<<"0x"<<std::hex<<std::setfill('0')<<std::setw(8)<<n; return out.str();
}
void key(std::ostream& out,const pkg::ResourceKey& k) {
    out<<"{\"instance\":"<<text(hex(k.instance))<<",\"type\":"<<text(hex(k.type))<<",\"group\":"<<text(hex(k.group))<<'}';
}
template<class T> void number(std::ostream& out,const std::optional<T>& n) {
    if(n) out<<+*n; else out<<"null";
}
void star_fields(std::ostream& out,const StarRecord& star) {
    out<<"{\"key\":"; if(star.key)out<<text(hex(*star.key));else out<<"null";
    out<<",\"flags\":";number(out,star.flags);
    out<<",\"tech_level\":";number(out,star.tech_level);
    out<<",\"type\":";number(out,star.type);
    out<<",\"planet_count\":";number(out,star.planet_count);
    out<<",\"name\":";if(star.name)out<<text(capture::utf8(*star.name));else out<<"null";
    out<<",\"position\":";
    if(star.position) {
        out<<'[';
        for(size_t i=0;i<3;++i) {
            const float v=(*star.position)[i];
            if(!std::isfinite(v))throw std::runtime_error("nonfinite serialized StarRecord position");
            if(i)out<<',';
            out<<v;
        }
        out<<']';
    } else out<<"null";
    out<<'}';
}
const char* resolution(ReferenceResolution s) {
    switch(s) {
    case ReferenceResolution::Null:return "null_reference";
    case ReferenceResolution::MissingClassStream:return "missing_class_stream";
    case ReferenceResolution::MissingObject:return "missing_object";
    case ReferenceResolution::UnsupportedClass:return "unsupported_class";
    case ReferenceResolution::StarFields:return "star_fields";
    }
    throw std::runtime_error("invalid reference resolution");
}
void rows(std::ostream& out,const PhysicalContextResult& ctx,const std::optional<ReferenceList>& refs,
          ExplicitDbpfReader& planets,const std::map<size_t,std::string>& headers,bool saved) {
    out<<"{\"status\":"<<text(refs?"observed_persisted_list":"absent_or_zero_size_field")<<",\"rows\":[";
    if(refs)for(size_t i=0;i<refs->size();++i) {
        if(i)out<<',';
        const auto& ref=(*refs)[i];
        const ClassObjects* cls=nullptr;
        for(const auto& c:ctx.decoded_classes)if(c.class_id==ref.class_id) {
            if(cls)throw std::runtime_error("ambiguous class stream");
            cls=&c;
        }
        ResolvedReference resolved;
        if(ref.is_null())resolved.status=ReferenceResolution::Null;
        else if(cls)resolved=resolve_star_reference(ref,*cls);
        out<<"{\"source_index\":"<<i<<",\"token\":"<<text(hex(ref.token))<<",\"class\":"<<text(hex(ref.class_id))
           <<",\"resolution\":"<<text(resolution(resolved.status))<<",\"terrain_query\":\"Unknown\"";
        const auto binding=headers.find(i);
        if(resolved.status!=ReferenceResolution::StarFields) {
            if(saved&&binding!=headers.end())throw std::runtime_error("header binding requires a resolved saved StarRecord");
            out<<",\"star\":null}";continue;
        }
        if(!cls||resolved.object_index>=cls->objects.size()||!cls->objects[resolved.object_index].star)
            throw std::runtime_error("inconsistent StarRecord resolution");
        const auto& star=*cls->objects[resolved.object_index].star;
        out<<",\"object_index\":"<<resolved.object_index<<",\"star\":";star_fields(out,star);
        if(saved) {
            std::vector<planet::ResourceRecord> records;
            planet::Selection selection;std::string error;
            const bool read=planet::read_selected_records(planets,star,records,error);
            const bool selected=read&&planet::select_save_stem(star,records,selection,error)&&selection.source_index.has_value();
            out<<",\"planet_selection\":{\"status\":"<<text(selected?"selected":read&&error.empty()?"empty_or_filtered":"blocked");
            if(selected) {
                out<<",\"source_index\":"<<*selection.source_index<<",\"resource\":";key(out,records[*selection.source_index].resource);
                out<<",\"save_stem\":"<<text(capture::utf8(selection.save_stem));
            }
            out<<",\"error\":"<<text(error)<<",\"records\":[";
            if(read)for(size_t j=0;j<records.size();++j) {
                if(j)out<<',';
                const auto& record=records[j];
                out<<"{\"resource\":";key(out,record.resource);
                out<<",\"transport\":"<<text(record.envelope?"observed_prefix_nested_dbpf":"direct_counted_payload");
                if(record.envelope) {
                    const auto& e=*record.envelope;
                    out<<",\"prefix_words\":["<<e.prefix_first<<','<<e.prefix_second<<"],\"inner_resource\":";key(out,e.inner_resource);
                    out<<",\"inner_container_bytes\":"<<e.inner_container_bytes<<",\"inner_index_end\":"<<e.inner_index_end
                       <<",\"unvalidated_container_tail_bytes\":"<<e.container_tail_bytes;
                }
                out<<'}';
            }
            out<<"]},\"header\":";
            if(binding==headers.end())out<<"{\"status\":\"Unknown\",\"reason\":\"no explicit saved-index binding\"}";
            else {
                if(!selected)throw std::runtime_error("header binding requires selected planet/save stem");
                const auto path=capture::path(binding->second);
                if(path.stem().u16string()!=selection.save_stem)
                    throw std::runtime_error("explicit header filename does not match selected save stem");
                ExplicitDbpfReader save_reader;save::FileHeader header;save::HeaderProbe probe;
                if(!save_reader.open(binding->second,error)||!save::read_selected_header(save_reader,header,error)||
                   !save::header_probe(header,probe,error))throw std::runtime_error("explicit save header blocked: "+error);
                out<<"{\"status\":\"decoded_header_subset\",\"binding\":\"caller_assigned_saved_index\",\"path\":"<<text(binding->second)
                   <<",\"filename_stem_matches_selection\":true,\"major\":"<<probe.words.first<<",\"minor\":"<<probe.words.second
                   <<",\"unsigned_major_ge_33\":"<<(probe.predicate?"true":"false")<<",\"unvalidated_root_tail\":"<<header.unvalidated_tail
                   <<",\"ownership_verified\":false}";
            }
        }
        out<<'}';
    }
    out<<"]}";
}
int run(const std::vector<std::string>& args) {
    if(args.size()<4||(args.size()-4)%3) {
        std::cerr<<"usage: starsdb_gge_capture <explicit-stars.db> <explicit-planetRecords.pkp> <NEW-observations.json> [--save-header <saved-index-decimal> <explicit-save.spo>]...\n";return 2;
    }
    try {
        std::map<size_t,std::string> headers;std::set<std::filesystem::path> paths;
        for(size_t i=4;i<args.size();i+=3) {
            size_t index=0;const auto& word=args[i+1];
            const auto parsed=std::from_chars(word.data(),word.data()+word.size(),index);
            if(args[i]!="--save-header"||word.empty()||parsed.ec!=std::errc{}||parsed.ptr!=word.data()+word.size()||index>=4096||
               !headers.emplace(index,args[i+2]).second)throw std::runtime_error("invalid/duplicate saved-index header binding");
            const auto path=std::filesystem::canonical(capture::path(args[i+2]));
            if(!paths.insert(path).second)throw std::runtime_error("same explicit header path assigned to multiple rows");
            for(const auto& existing:paths)if(existing!=path&&std::filesystem::equivalent(existing,path))
                throw std::runtime_error("same header file assigned through filesystem aliases");
        }
        const auto output=capture::path(args[3]);
        if(std::filesystem::exists(output))throw std::runtime_error("output must be NEW; existing paths are never overwritten");
        std::string error;PhysicalContextResult ctx;
        if(!inspect_physical_context(args[1],save::kRoot,save::kRoot,save::kRoot,ctx,error))throw std::runtime_error("stars.db blocked: "+error);
        if(!ctx.manager_links)throw std::runtime_error("manager links absent");
        for(const auto& [index,path]:headers)if(!ctx.manager_links->saved||index>=ctx.manager_links->saved->size())
            throw std::runtime_error("header binding saved index outside observed list");
        if((ctx.manager_links->saved&&ctx.manager_links->saved->size()>4096)||
           (ctx.manager_links->starters&&ctx.manager_links->starters->size()>4096))throw std::runtime_error("capture list exceeds 4096-row host bound");
        ExplicitDbpfReader planets;if(!planets.open(args[2],error))throw std::runtime_error("planet package blocked: "+error);
        std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<float>::max_digits10)<<std::showpoint;
        out<<"{\"format\":\"GGE_SAVE_OBSERVATIONS_V1\",\"authority\":\"structural save bytes; not native runtime replay\","
           <<"\"stars_path\":"<<text(args[1])<<",\"planets_path\":"<<text(args[2])
           <<",\"production_catalogue\":false,\"terrain_query\":\"Unknown\",\"runtime_refill\":\"Unknown\","
           <<"\"post_load_fixups\":\"Unknown\",\"physical_context\":{\"status\":\"decoded_structural_subset\",\"root\":";
        key(out,ctx.root.key);out<<",\"index\":";key(out,ctx.index.key);
        out<<",\"class_streams\":"<<ctx.decoded_classes.size()<<"},\"persisted_saved\":";
        rows(out,ctx,ctx.manager_links->saved,planets,headers,true);
        out<<",\"persisted_starters\":";rows(out,ctx,ctx.manager_links->starters,planets,{},false);
        out<<"}\n";
        capture::publish_new(output,out.str());
        std::cout<<"GGE_SAVE_OBSERVATIONS_V1 written; terrain=Unknown runtime_refill=Unknown production_catalogue=false\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"capture blocked: "<<e.what()<<'\n';return 1;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    try {
        std::vector<std::string> args;
        static_assert(sizeof(wchar_t)==sizeof(char16_t));
        for(int i=0;i<argc;++i) {
            std::u16string units;
            for(const wchar_t* p=argv[i];*p;++p)units.push_back(static_cast<char16_t>(*p));
            args.push_back(capture::utf8(units));
        }
        return run(args);
    }catch(const std::exception& e){std::cerr<<"capture blocked: "<<e.what()<<'\n';return 1;}
}
#else
int main(int argc,char** argv) { return run(std::vector<std::string>(argv,argv+argc)); }
#endif
