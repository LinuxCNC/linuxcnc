"""GMI ToolTable client — REST interface to the tool table shared memory."""

import json
import urllib.request

import gmi


class ToolTable:
    """REST client for the tool table API.

    Provides list/get/put/delete/reload operations mirroring the
    GET/PUT/DELETE /api/v1/tools/* endpoints served by milltask.
    """

    def __init__(self, instance: str = "milltask"):
        self._base = gmi.rest_url() + "/api/v1/" + instance

    def list(self):
        """Return all tools as a list of dicts."""
        with urllib.request.urlopen(self._base + "/", timeout=5) as resp:
            return json.loads(resp.read())

    def get(self, toolno):
        """Return a single tool dict by tool number, or None if not found."""
        url = self._base + "/%d" % toolno
        try:
            with urllib.request.urlopen(url, timeout=5) as resp:
                return json.loads(resp.read())
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            raise

    def put(self, toolno, entry):
        """Create or update a tool. entry is a dict of tool fields."""
        entry["toolno"] = toolno
        data = json.dumps(entry).encode("utf-8")
        req = urllib.request.Request(
            self._base + "/%d" % toolno,
            data=data,
            headers={"Content-Type": "application/json"},
            method="PUT",
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read())

    def delete(self, toolno):
        """Delete a tool by tool number."""
        req = urllib.request.Request(
            self._base + "/%d" % toolno,
            method="DELETE",
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read())

    def reload(self):
        """Reload the tool table from file (sends NML load-tool-table command)."""
        req = urllib.request.Request(
            self._base + "/reload",
            method="POST",
            headers={"Content-Type": "application/json"},
            data=b"{}",
        )
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read())
