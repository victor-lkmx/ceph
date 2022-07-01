
#pragma once

#include "include/encoding.h"

#include "rgw_sync_info.h"

namespace ceph {
  class Formatter;
}

class RGWMetadataManager;

struct siprovider_data_info : public SIProvider::EntryInfoBase {
  std::string key;
  int shard_id{-1}; /* -1: not a specific shard, entry refers to all the shards */
  int num_shards{0};
  std::optional<ceph::real_time> timestamp;

  siprovider_data_info() {}
  siprovider_data_info(const std::string& _key,
                       int _shard_id,
                       int _num_shards,
                       std::optional<ceph::real_time> _ts) : key(_key),
                                                             shard_id(_shard_id),
                                                             num_shards(_num_shards),
                                                             timestamp(_ts) {}

  std::string get_data_type() const override {
    return "data";
  }

  void encode(bufferlist& bl) const override {
    ENCODE_START(1, 1, bl);
    encode(key, bl);
    encode(shard_id, bl);
    encode(num_shards, bl);
    encode(timestamp, bl);
    ENCODE_FINISH(bl);
  }

  void decode(bufferlist::const_iterator& bl) override {
     DECODE_START(1, bl);
     decode(key, bl);
     decode(shard_id, bl);
     decode(num_shards, bl);
     decode(timestamp, bl);
     DECODE_FINISH(bl);
  }

  void dump(Formatter *f) const override;
  void decode_json(JSONObj *obj) override;
};
WRITE_CLASS_ENCODER(siprovider_data_info)

class RGWDatadataManager;
class RGWBucketCtl;

class SIProvider_DataFull : public SIProvider_SingleStage
{
  struct {
    RGWMetadataManager *mgr;
  } meta;

  struct {
    RGWBucketCtl *bucket;
  } ctl;

protected:
  int do_fetch(const DoutPrefixProvider *dpp,
               int shard_id, std::string marker, int max, fetch_result *result) override;

  int do_get_start_marker(const DoutPrefixProvider *dpp,
                          int shard_id, std::string *marker, ceph::real_time *timestamp) const override {
    marker->clear();
    *timestamp = ceph::real_time();
    return 0;
  }

  int do_get_cur_state(const DoutPrefixProvider *dpp,
                       int shard_id, std::string *marker, ceph::real_time *timestamp, bool *disabled, optional_yield y) const {
    marker->clear(); /* full data, no current incremental state */
    *timestamp = ceph::real_time();
    *disabled = false;
    return 0;
  }


  int do_trim(const DoutPrefixProvider *dpp,
              int shard_id, const std::string& marker) override {
    return 0;
  }

public:
  SIProvider_DataFull(CephContext *_cct,
                      RGWMetadataManager *meta_mgr,
                      RGWBucketCtl *_bucket_ctl) : SIProvider_SingleStage(_cct,
									  "data.full",
                                                                          std::nullopt,
                                                                          std::make_shared<SITypeHandlerProvider_Default<siprovider_data_info> >(),
                                                                          std::nullopt, /* stage id */
									  SIProvider::StageType::FULL,
									  1,
                                                                          false) {
    meta.mgr = meta_mgr;
    ctl.bucket = _bucket_ctl;
  }

  int init(const DoutPrefixProvider *dpp) {
    return 0;
  }

};

class RGWDataChangesLog;

class SIProvider_DataInc : public SIProvider_SingleStage
{
  struct {
    RGWDataChangesLog *datalog;
  } svc;

  struct {
    RGWBucketCtl *bucket;
  } ctl;

  RGWDataChangesLog *data_log{nullptr};

protected:
  int do_fetch(const DoutPrefixProvider *dpp,
               int shard_id, std::string marker, int max, fetch_result *result) override;

  int do_get_start_marker(const DoutPrefixProvider *dpp,
                          int shard_id, std::string *marker, ceph::real_time *timestamp) const override;
  int do_get_cur_state(const DoutPrefixProvider *dpp,
                       int shard_id, std::string *marker, ceph::real_time *timestamp, bool *disabled, optional_yield y) const;

  int do_trim(const DoutPrefixProvider *dpp,
              int shard_id, const std::string& marker) override;
public:
  SIProvider_DataInc(CephContext *_cct,
                     RGWDataChangesLog *_datalog_svc,
                     RGWBucketCtl *_bucket_ctl);

  int init(const DoutPrefixProvider *dpp);
};