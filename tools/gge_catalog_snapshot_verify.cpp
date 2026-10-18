// Caller-selected schema/atomicity test; no profile search or original data.
#include "engine/gge_catalog_snapshot.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace os::gge;
static bool write_file(const std::filesystem::path& p,const std::string& s){std::ofstream f(p,std::ios::binary);if(!f)return false;f<<s;return !!f;}
int main(int argc,char** argv){
 if(argc!=2){std::cerr<<"usage: gge_catalog_snapshot_verify <NEW-test-dir>\n";return 2;}
 std::error_code ec;std::filesystem::path dir=argv[1];if(std::filesystem::exists(dir,ec)||!std::filesystem::create_directory(dir,ec)){std::cerr<<"directory must be new\n";return 2;}
 size_t checks=0;auto check=[&](bool ok,const char* why){++checks;if(!ok)std::cerr<<why<<'\n';return ok;};std::string e;
  const std::string valid="GGE_CATALOG_V1\n# caller-selected capture\nsaved_status\tAvailable\ncandidate_status\tAvailable\nmanager_status\tAvailable\nstarsdb_status\tAvailable\nplanet_status\tAvailable\nrefill_status\tAvailable\nresource_root_status\tAvailable\nresource_index_status\tUnknown\nresource_context_status\tFailed\nresource_class_status\tAvailable\nrefill_append\tAppended\nsaved_count\t1\ncandidate_count\t2\n"
  "saved\t42\t1\t2\t3\t5\t0\t536176656420F09F8C90\t5363617065\t1\t37\t3\t1\t0xB89D2CD2\n"
  "candidate\t100\t-1\t0\t0\t5\t0\t43616E64696461746520E29885\t0\t0\n"
  "candidate\t101\t1\t0\t0\t5\t0\t43616E6469646174652042\t1\t0xB89D2CD2\nEND\n";
  auto path=dir/"valid.txt";if(!check(write_file(path,valid),"write valid"))return 1;CatalogInput out;if(!check(load_catalog_snapshot(path.string(),out,e)&&out.saved_status==CatalogAvailability::Available&&out.candidate_status==CatalogAvailability::Available&&out.saved.size()==1&&out.candidates.size()==2&&out.producer.resource_root==CatalogProducerStatus::Available&&out.producer.resource_index==CatalogProducerStatus::Unknown&&out.producer.resource_context==CatalogProducerStatus::Failed&&out.producer.refill_append==CatalogRefillOutcome::Appended,"valid saved/candidate snapshot with optional producer detail"))return 1;
 const std::u16string expected_name={u'S',u'a',u'v',u'e',u'd',u' ',char16_t(0xd83c),char16_t(0xdf10)};
 const bool fields=out.saved[0].star.star_id==42&&out.saved[0].star.name==expected_name&&out.saved[0].save_stem==u"Scape"&&out.saved[0].header.first==37&&out.saved[0].star.terrain_image==0xb89d2cd2u&&out.candidates[1].terrain_loaded&&out.candidates[1].terrain_image==0xb89d2cd2u;
 if(!fields){std::cerr<<"id="<<out.saved[0].star.star_id<<" name_units="<<out.saved[0].star.name.size()<<" stem_units="<<out.saved[0].save_stem.size()<<" header="<<out.saved[0].header.first<<" terrain="<<out.candidates[1].terrain_loaded<<" nameeq="<<(out.saved[0].star.name==expected_name)<<" stemeq="<<(out.saved[0].save_stem==u"Scape")<<" units=";for(auto c:out.saved[0].save_stem)std::cerr<<std::hex<<(unsigned)c<<',';std::cerr<<" error="<<e<<'\n';}
 if(!check(fields,"UTF8 hex names/fields"))return 1;
 CatalogInput sentinel=out;auto test=[&](const std::string& suffix,const std::string& body){auto p=dir/suffix;return write_file(p,body)&&!load_catalog_snapshot(p.string(),out,e)&&out.saved.size()==sentinel.saved.size()&&out.candidates.size()==sentinel.candidates.size();};
  auto unknown_path=dir/"unknown.txt"; if(!check(write_file(unknown_path,"GGE_CATALOG_V1\nsaved_status\tUnknown\ncandidate_status\tAvailable\nmanager_status\tUnknown\nstarsdb_status\tUnknown\nplanet_status\tUnknown\nrefill_status\tUnknown\nsaved_count\t0\ncandidate_count\t0\nEND\n")&&load_catalog_snapshot(unknown_path.string(),out,e)&&out.saved_status==CatalogAvailability::Unknown&&out.producer.manager==CatalogProducerStatus::Unknown&&out.producer.resource_root==CatalogProducerStatus::Unknown&&out.producer.refill_append==CatalogRefillOutcome::Unknown,"Unknown source remains explicit and optional provenance defaults Unknown"))return 1;
 out=sentinel;if(!check(test("line.txt","GGE_CATALOG_V1\nsaved_status\tAvailable\ncandidate_status\tAvailable\nsaved_count\t0\ncandidate_count\t0\nwat\tunknown\nEND\n"),"unknown line rejected"))return 1;
  if(!check(test("row.txt","GGE_CATALOG_V1\nsaved_status\tAvailable\ncandidate_status\tAvailable\nsaved_count\t1\ncandidate_count\t0\nsaved\t1\t0\t0\t0\t5\t0\t00\nEND\n"),"short row rejected"))return 1;
 if(!check(test("utf.txt","GGE_CATALOG_V1\nsaved_status\tAvailable\ncandidate_status\tAvailable\nsaved_count\t0\ncandidate_count\t1\ncandidate\t1\tNaN\t0\t0\t5\t0\t41\t0\t0\nEND\n"),"nonfinite coordinate rejected"))return 1;
 if(!check(test("negative.txt","GGE_CATALOG_V1\nsaved_status\tAvailable\ncandidate_status\tAvailable\nsaved_count\t0\ncandidate_count\t1\ncandidate\t-1\t0\t0\t0\t5\t0\t41\t0\t0\nEND\n"),"negative unsigned field rejected"))return 1;
  if(!check(test("terrain.txt","GGE_CATALOG_V1\nsaved_status\tAvailable\ncandidate_status\tAvailable\nsaved_count\t0\ncandidate_count\t1\ncandidate\t1\t0\t0\t0\t5\t0\t41\t1\t0\nEND\n"),"loaded terrain without image rejected"))return 1;
  if(!check(test("optional-duplicate.txt","GGE_CATALOG_V1\nsaved_status\tAvailable\ncandidate_status\tAvailable\nmanager_status\tAvailable\nstarsdb_status\tAvailable\nplanet_status\tAvailable\nrefill_status\tAvailable\nresource_root_status\tAvailable\nresource_root_status\tFailed\nsaved_count\t0\ncandidate_count\t0\nEND\n"),"duplicate optional producer status rejected"))return 1;
  if(!check(test("optional-invalid-outcome.txt","GGE_CATALOG_V1\nsaved_status\tAvailable\ncandidate_status\tAvailable\nmanager_status\tAvailable\nstarsdb_status\tAvailable\nplanet_status\tAvailable\nrefill_status\tAvailable\nrefill_append\tMaybe\nsaved_count\t0\ncandidate_count\t0\nEND\n"),"invalid optional refill outcome rejected"))return 1;
 auto bom=std::string("\xef\xbb\xbf")+valid;auto bp=dir/"bom.txt";if(!check(write_file(bp,bom)&&load_catalog_snapshot(bp.string(),out,e),"UTF8 BOM accepted"))return 1;
 std::filesystem::remove_all(dir,ec);std::cout<<checks<<" catalogue snapshot schema/atomic/UTF8/status checks PASS\n";return 0;
}
