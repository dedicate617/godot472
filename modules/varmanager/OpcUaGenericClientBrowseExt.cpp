// OpcUaGenericClientBrowseExt.cpp
// Implements OPCUA::GenericClient::browseChildren() as a member function so it
// can access the private UA_Client* without modifying the pre-built library.

#include <OpcUaGenericClient.hh>
#include <open62541/client.h>
#include <open62541/client_highlevel.h>
#include <mutex>

namespace OPCUA {

std::vector<std::pair<std::string, int>>
GenericClient::browseChildren(const std::string &nodePath) const
{
    std::vector<std::pair<std::string, int>> result;
    if (!client) return result;

    // P4: UA_Client* is NOT thread-safe. This browse runs on the GD calling
    // thread, while the background run thread is concurrently in run_once()
    // (which locks clientMutex). Without holding the same lock here, the two
    // race on the secure channel -> BadSecurityChecksFailed -> the channel
    // drops mid-enumeration and the process crashes. Hold clientMutex for the
    // whole browse (incl. the browseNodePath resolution and the browseNext
    // pagination loop) so browse and run_once are mutually exclusive.
    // recursive_mutex: browseNodePath()/findElement() do not re-lock, but the
    // recursive type is harmless and matches run_once()'s lock.
    std::unique_lock<std::recursive_mutex> lock(clientMutex);

    UA_NodeId parentId;
    if (nodePath.empty()) {
        parentId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    } else {
        NodeId resolved = browseNodePath(nodePath);
        if (resolved.isNull()) return result;
        parentId = resolved.getNativeIdCopy();
    }

    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId          = parentId;
    bReq.nodesToBrowse[0].resultMask      = UA_BROWSERESULTMASK_ALL;
    bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_FORWARD;
    bReq.nodesToBrowse[0].includeSubtypes = UA_TRUE;
    bReq.nodesToBrowse[0].referenceTypeId =
        UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);

    UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);
    UA_BrowseRequest_clear(&bReq);

    // Collect references, following continuation points if the server paginates
    for (;;) {
        UA_ByteString continuation = UA_BYTESTRING_NULL;
        for (size_t i = 0; i < bResp.resultsSize; ++i) {
            UA_BrowseResult *br = &bResp.results[i];
            for (size_t j = 0; j < br->referencesSize; ++j) {
                UA_ReferenceDescription *ref = &br->references[j];
                std::string name(
                    reinterpret_cast<const char*>(ref->browseName.name.data),
                    ref->browseName.name.length);
                result.push_back({name, static_cast<int>(ref->nodeClass)});
            }
            if (br->continuationPoint.length > 0) {
                UA_ByteString_copy(&br->continuationPoint, &continuation);
            }
        }

        if (continuation.length == 0) {
            break;
        }

        UA_BrowseNextRequest nextReq;
        UA_BrowseNextRequest_init(&nextReq);
        nextReq.releaseContinuationPoints = UA_FALSE;
        nextReq.continuationPoints = &continuation;
        nextReq.continuationPointsSize = 1;
        UA_BrowseResponse_clear(&bResp);
        *reinterpret_cast<UA_BrowseNextResponse*>(&bResp) =
            UA_Client_Service_browseNext(client, nextReq);
        UA_ByteString_clear(&continuation);
    }

    UA_BrowseResponse_clear(&bResp);
    return result;
}

} // namespace OPCUA
