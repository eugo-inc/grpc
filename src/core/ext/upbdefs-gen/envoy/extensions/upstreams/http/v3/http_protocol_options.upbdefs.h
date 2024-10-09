/* This file was generated by upb_generator from the input file:
 *
 *     envoy/extensions/upstreams/http/v3/http_protocol_options.proto
 *
 * Do not edit -- your changes will be discarded when the file is
 * regenerated.
 * NO CHECKED-IN PROTOBUF GENCODE */

#ifndef ENVOY_EXTENSIONS_UPSTREAMS_HTTP_V3_HTTP_PROTOCOL_OPTIONS_PROTO_UPBDEFS_H_
#define ENVOY_EXTENSIONS_UPSTREAMS_HTTP_V3_HTTP_PROTOCOL_OPTIONS_PROTO_UPBDEFS_H_

#include "upb/reflection/def.h"
#include "upb/reflection/internal/def_pool.h"

#include "upb/port/def.inc" // Must be last.
#ifdef __cplusplus
extern "C" {
#endif

extern _upb_DefPool_Init envoy_extensions_upstreams_http_v3_http_protocol_options_proto_upbdefinit;

UPB_INLINE const upb_MessageDef *envoy_extensions_upstreams_http_v3_HttpProtocolOptions_getmsgdef(upb_DefPool *s) {
  _upb_DefPool_LoadDefInit(s, &envoy_extensions_upstreams_http_v3_http_protocol_options_proto_upbdefinit);
  return upb_DefPool_FindMessageByName(s, "envoy.extensions.upstreams.http.v3.HttpProtocolOptions");
}

UPB_INLINE const upb_MessageDef *envoy_extensions_upstreams_http_v3_HttpProtocolOptions_ExplicitHttpConfig_getmsgdef(upb_DefPool *s) {
  _upb_DefPool_LoadDefInit(s, &envoy_extensions_upstreams_http_v3_http_protocol_options_proto_upbdefinit);
  return upb_DefPool_FindMessageByName(s, "envoy.extensions.upstreams.http.v3.HttpProtocolOptions.ExplicitHttpConfig");
}

UPB_INLINE const upb_MessageDef *envoy_extensions_upstreams_http_v3_HttpProtocolOptions_UseDownstreamHttpConfig_getmsgdef(upb_DefPool *s) {
  _upb_DefPool_LoadDefInit(s, &envoy_extensions_upstreams_http_v3_http_protocol_options_proto_upbdefinit);
  return upb_DefPool_FindMessageByName(s, "envoy.extensions.upstreams.http.v3.HttpProtocolOptions.UseDownstreamHttpConfig");
}

UPB_INLINE const upb_MessageDef *envoy_extensions_upstreams_http_v3_HttpProtocolOptions_AutoHttpConfig_getmsgdef(upb_DefPool *s) {
  _upb_DefPool_LoadDefInit(s, &envoy_extensions_upstreams_http_v3_http_protocol_options_proto_upbdefinit);
  return upb_DefPool_FindMessageByName(s, "envoy.extensions.upstreams.http.v3.HttpProtocolOptions.AutoHttpConfig");
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#include "upb/port/undef.inc"

#endif  /* ENVOY_EXTENSIONS_UPSTREAMS_HTTP_V3_HTTP_PROTOCOL_OPTIONS_PROTO_UPBDEFS_H_ */
