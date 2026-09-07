# ezpilot: Standalone offline mode - external Kommu cloud endpoints neutralized


class AuthException(Exception):
  pass


def refresh_session():
  pass


def kapi(func, *args, **kwargs):
  raise AuthException("External Kommu cloud API disabled in ezpilot standalone mode")


