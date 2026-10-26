#pragma once

#include <cstdint>
#include <ctime>
#include <functional>
#include <string>

#include <smb2/smb2.h>

struct smb2_context;

namespace react_native_smb::listing {

// Enumerates one directory with raw async PDUs on a caller-serviced context:
//
//   [CREATE + QUERY_DIRECTORY] (compound) → QUERY_DIRECTORY … → NO_MORE_FILES → CLOSE
//
// Each reply buffer is handed to the chunk handler while it is valid. The done
// handler fires exactly once, before the CLOSE is sent, so callers never wait on
// the CLOSE round trip to use the result.
//
// Callbacks reference this object: keep it alive until idle(), or until the
// context has been destroyed.
class DirQuery {
   public:
    // Return false when the buffer is malformed.
    using ChunkHandler = std::function<bool(const uint8_t* buffer, uint32_t length)>;
    // 0 = fully enumerated; otherwise the failing NT status.
    using DoneHandler = std::function<void(uint32_t status)>;

    DirQuery(smb2_context* ctx, std::string path, ChunkHandler onChunk, DoneHandler onDone);
    DirQuery(const DirQuery&) = delete;
    DirQuery& operator=(const DirQuery&) = delete;

    // Queues the opening compound. Returns false if nothing could be queued.
    bool start();
    // Finish after the in-flight reply (done status STATUS_CANCELLED) and close the handle.
    void stop() { stopped_ = true; }

    bool done() const { return done_; }
    // No libsmb2 callback is pending for this query.
    bool idle() const { return outstanding_ == 0; }

   private:
    static void onCreate(smb2_context* smb2, int status, void* commandData, void* privateData);
    static void onQuery(smb2_context* smb2, int status, void* commandData, void* privateData);
    static void onClose(smb2_context* smb2, int status, void* commandData, void* privateData);

    void handleQueryReply(uint32_t status, void* commandData);
    bool queueQuery();
    void queueClose();
    void finish(uint32_t status);

    smb2_context* ctx_;
    std::string path_;
    ChunkHandler onChunk_;
    DoneHandler onDone_;

    smb2_file_id fileId_{};
    bool opened_{false};
    uint32_t createStatus_{0};
    bool firstQuery_{true};
    bool stopped_{false};
    bool done_{false};
    int outstanding_{0};
};

}  // namespace react_native_smb::listing
