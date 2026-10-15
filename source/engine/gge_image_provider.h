#pragma once
#include "engine/gge_star_phase1.h"
#include "engine/starsdb_dbpf.h"

namespace os::gge {
enum class ImageReadStatus { Unknown, Found, Missing, Ambiguous, Unsupported, ReadFailure };
struct ImageRead {
    ImageReadStatus status=ImageReadStatus::Unknown;
    spui::ImageBinding image;
    std::string error;
};
class ImageProvider {
public:
    virtual ~ImageProvider()=default;
    virtual ImageRead read(const ImageKey& key)=0;
};
// Bounded PNG-only provider: one explicitly selected DBPF, strict existing 3.0/index3 subset,
// no resource precedence/profile scan. Found means actual decoded BGRA, not just matching index.
class ExplicitImageProvider final:public ImageProvider {
public:
    bool open(const std::string& path,std::string& error);
    bool open_from_memory(const std::vector<u8>& bytes,std::string& error);
    ImageRead read(const ImageKey& key) override;
    const pkg::Package& package() const { return reader_.package(); }
private:
    starsdb::ExplicitDbpfReader reader_;
    std::string label_;
};
enum class ImageApplyStatus { Applied, Blocked, Missing, Ambiguous, Unsupported, ReadFailure };
struct ImageApplyResult { ImageApplyStatus status=ImageApplyStatus::Blocked; std::string error; };
// Applies ONE resolved ImageResource/StageImageTarget request. No phase completion, no other
// actions skipped as if successful. Stage source needs an explicit target choice and getter image;
// an unloaded descriptor is read through provider. Unsupported getter blocks, no guessed fallback.
ImageApplyResult apply_star_image(StarUiInstance& instance,u64 plan_instance_id,
                                  const Phase1Request& request,ImageProvider& provider);
}  // namespace os::gge
