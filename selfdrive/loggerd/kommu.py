# ezpilot: Standalone offline mode - external Kommu FIA uploads disabled


class OfflineUploadResponse:
  status_code = 200


def fia_upload(base_fn, fn):
  # In ezpilot standalone mode, uploads to web.kommu.ai are eliminated
  return OfflineUploadResponse()

