"""GMI client package — generated REST and WebSocket clients for LinuxCNC."""

import os

_DEFAULT_REST_URL = "http://127.0.0.1:5080"
_ENV_VAR = "GMC_REST_URL"
_INSTANCE_ENV_VAR = "GMC_INSTANCE"
_DEFAULT_INSTANCE = "milltask"

# Version string (matches linuxcnc.version).
version = os.environ.get("LINUXCNCVERSION", "unknown")


def instance() -> str:
    """Return the target instance name (from GMC_INSTANCE or default 'milltask')."""
    return os.environ.get(_INSTANCE_ENV_VAR, _DEFAULT_INSTANCE)


def preview_instance() -> str:
    """Return the preview instance name.

    If GMC_PREVIEW_INSTANCE is set, use it.
    Otherwise derive from GMC_INSTANCE: '{instance}-preview'.
    If neither is set, fall back to 'ngcpreview'.
    """
    if "GMC_PREVIEW_INSTANCE" in os.environ:
        return os.environ["GMC_PREVIEW_INSTANCE"]
    inst = os.environ.get(_INSTANCE_ENV_VAR)
    return f"{inst}-preview" if inst else "ngcpreview"


def mtc_instance() -> str:
    """Return the manual-tool-change instance name.

    If GMC_MTC_INSTANCE is set, use it.
    Otherwise derive from GMC_INSTANCE: '{instance}-manualtoolchange'.
    If neither is set, fall back to 'manualtoolchange'.
    """
    if "GMC_MTC_INSTANCE" in os.environ:
        return os.environ["GMC_MTC_INSTANCE"]
    inst = os.environ.get(_INSTANCE_ENV_VAR)
    return f"{inst}-manualtoolchange" if inst else "manualtoolchange"


def rest_url() -> str:
    """Return the REST base URL (from GMC_REST_URL or default)."""
    return os.environ.get(_ENV_VAR, _DEFAULT_REST_URL).rstrip("/")


def ws_url() -> str:
    """Return the WebSocket watch URL derived from the REST URL."""
    base = rest_url()
    base = base.replace("https://", "wss://").replace("http://", "ws://")
    return base + "/api/v1/watch"


# Re-export wrapper classes for convenience.
# These are lazy-imported to avoid pulling in websockets at module load
# (not all callers need stat/error channels).
def Stat():
    """Create a gmi.Stat instance (drop-in for linuxcnc.stat())."""
    from gmi.stat import Stat as _Stat
    return _Stat(instance=instance())


def Command():
    """Create a gmi.Command instance (drop-in for linuxcnc.command())."""
    from gmi.command import Command as _Command
    return _Command(instance=instance())


def ErrorChannel():
    """Create a gmi.ErrorChannel instance (drop-in for linuxcnc.error_channel())."""
    from gmi.error import ErrorChannel as _ErrorChannel
    return _ErrorChannel(instance=instance())


def MessageList(on_update=None):
    """Create a gmi.MessageList instance for the server-side message list."""
    from gmi.messages import MessageList as _MessageList
    return _MessageList(instance=instance(), on_update=on_update)


def positionlogger(stat_unused, c0, c1, c2, c3, c4, c5, geometry, is_xyuv=0):
    """Create a gmi.PositionLogger (drop-in for linuxcnc.positionlogger())."""
    from gmi.positionlogger import PositionLogger
    return PositionLogger(stat_unused, c0, c1, c2, c3, c4, c5, geometry, is_xyuv)


def ToolTable():
    """Create a gmi.ToolTable instance for REST tool table access."""
    from gmi.tools import ToolTable as _ToolTable
    return _ToolTable(instance=instance())


def component_exists(name: str) -> bool:
    """Check if a HAL component exists via the halcmd REST API."""
    import json
    import urllib.request
    url = rest_url() + "/api/v1/halcmd/components?pattern=" + name
    try:
        with urllib.request.urlopen(url, timeout=2) as resp:
            data = json.loads(resp.read())
            return len(data) > 0
    except Exception:
        return False


def pin_has_writer(name: str) -> bool:
    """Check if a HAL pin's signal has any writers via the halcmd REST API."""
    import json
    import urllib.request
    url = rest_url() + "/api/v1/halcmd/pins?pattern=" + name
    try:
        with urllib.request.urlopen(url, timeout=2) as resp:
            data = json.loads(resp.read())
            if data:
                return data[0].get("has_writer", False)
            return False
    except Exception:
        return False


class IniFile:
    """Drop-in replacement for linuxcnc.ini() that fetches values via REST.

    Matches the linuxcnc.ini API:
      - find(section, key) -> str | None
      - findall(section, key) -> list[str]

    When GMC_INSTANCE is set (multi-instance), namespace-prefixed sections
    (e.g. [mill2:KINS]) are resolved automatically via the server.
    """

    def __init__(self):
        from gmi.ini_client import IniClient, IniQueryItem
        self._client = IniClient(rest_url())
        self._cache = {}  # (section, key) -> str or None (find)
        self._cache_all = {}  # (section, key) -> list[str] (findall)
        # Use namespace only when GMC_INSTANCE is explicitly set.
        ns = os.environ.get(_INSTANCE_ENV_VAR)
        self._namespace = ns if ns else None

    def find(self, section, key):
        """Return the first value for section/key, or None if not found."""
        cache_key = (section, key)
        if cache_key in self._cache:
            return self._cache[cache_key]
        from gmi.ini_client import IniQueryItem
        results = self._client.query([IniQueryItem(section=section, key=key, namespace=self._namespace).to_dict()])
        if results and len(results) == 1:
            val = results[0].value
            self._cache[cache_key] = val
            return val
        self._cache[cache_key] = None
        return None

    def findall(self, section, key):
        """Return all values for section/key as a list."""
        cache_key = (section, key)
        if cache_key in self._cache_all:
            return self._cache_all[cache_key]
        from gmi.ini_client import IniQueryItem
        results = self._client.query([IniQueryItem(section=section, key=key, all=True, namespace=self._namespace).to_dict()])
        if results and len(results) == 1:
            vals = results[0].values or []
            self._cache_all[cache_key] = vals
            return vals
        self._cache_all[cache_key] = []
        return []


def fetch_parameter_file():
    """Fetch the RS274NGC parameter file content from the REST service."""
    from gmi.ini_client import IniClient
    ns = os.environ.get(_INSTANCE_ENV_VAR)
    client = IniClient(rest_url())
    return client.get_parameter_file(namespace=ns if ns else None)
