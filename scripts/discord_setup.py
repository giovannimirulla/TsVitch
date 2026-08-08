#!/usr/bin/env python3
"""Create or update a private Discord category/channel for beta testers.

Example:
  python3 scripts/discord_setup.py \
    --token "$DISCORD_TOKEN" \
    --guild-id 123456789012345678 \
    --category-name "Beta Testers" \
    --channel-name "beta-testers" \
    --role-name "Beta Tester"
"""

import argparse
import json
import os
import sys
import urllib.error
import urllib.request
from typing import Any, Dict, Optional

BASE_URL = "https://discord.com/api/v10"


def build_headers(token: str, token_type: str) -> Dict[str, str]:
    auth_prefix = "Bot" if token_type.lower() == "bot" else "Bearer"
    return {
        "Authorization": f"{auth_prefix} {token}".strip(),
        "Content-Type": "application/json",
        "User-Agent": "TsVitch-Discord-Setup/1.0",
    }


def api_request(method: str, path: str, token: str, token_type: str, payload: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    url = f"{BASE_URL}{path}"
    data = None
    headers = build_headers(token, token_type)
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")

    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req) as response:
            body = response.read().decode("utf-8")
            return json.loads(body) if body else {}
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        try:
            detail = json.loads(body)
        except json.JSONDecodeError:
            detail = {"message": body}
        raise RuntimeError(f"Discord API error {exc.code}: {detail}") from exc


def get_or_create_role(guild_id: str, token: str, token_type: str, role_name: str) -> Dict[str, Any]:
    roles = api_request("GET", f"/guilds/{guild_id}/roles", token, token_type)
    for role in roles:
        if role.get("name", "").lower() == role_name.lower():
            return role

    payload = {
        "name": role_name,
        "permissions": "0",
        "color": 0,
        "hoist": False,
        "mentionable": False,
    }
    return api_request("POST", f"/guilds/{guild_id}/roles", token, token_type, payload)


def find_channel(channels: list[Dict[str, Any]], name: str, parent_id: Optional[str] = None) -> Optional[Dict[str, Any]]:
    for channel in channels:
        if channel.get("name") != name:
            continue
        if parent_id is None or channel.get("parent_id") == parent_id:
            return channel
    return None


def get_channels(guild_id: str, token: str, token_type: str) -> list[Dict[str, Any]]:
    return api_request("GET", f"/guilds/{guild_id}/channels", token, token_type)


def create_permission_overwrite(guild_id: str, token: str, token_type: str, channel_id: str, target_id: str, target_type: str, allow: int, deny: int) -> None:
    payload = {
        "type": target_type,
        "id": target_id,
        "allow": str(allow),
        "deny": str(deny),
    }
    api_request("PUT", f"/channels/{channel_id}/permissions/{target_id}", token, token_type, payload)


def ensure_private_channel(guild_id: str, token: str, token_type: str, category_name: str, channel_name: str, role_name: str) -> Dict[str, Any]:
    role = get_or_create_role(guild_id, token, token_type, role_name)
    role_id = role["id"]

    channels = get_channels(guild_id, token, token_type)
    category = find_channel(channels, category_name)

    if category is None:
        category_payload = {
            "name": category_name,
            "type": 4,
            "permission_overwrites": [
                {
                    "id": guild_id,
                    "type": "role",
                    "allow": 0,
                    "deny": 1024,
                },
                {
                    "id": role_id,
                    "type": "role",
                    "allow": 1024,
                    "deny": 0,
                },
            ],
        }
        category = api_request("POST", f"/guilds/{guild_id}/channels", token, token_type, category_payload)
    else:
        # Ensure the category has the right permissions.
        create_permission_overwrite(guild_id, token, token_type, category["id"], guild_id, "role", 0, 1024)
        create_permission_overwrite(guild_id, token, token_type, category["id"], role_id, "role", 1024, 0)

    channel = find_channel(channels, channel_name, parent_id=category["id"])
    if channel is None:
        channel_payload = {
            "name": channel_name,
            "type": 0,
            "parent_id": category["id"],
            "permission_overwrites": [
                {
                    "id": guild_id,
                    "type": "role",
                    "allow": 0,
                    "deny": 1024,
                },
                {
                    "id": role_id,
                    "type": "role",
                    "allow": 1024,
                    "deny": 0,
                },
            ],
        }
        channel = api_request("POST", f"/guilds/{guild_id}/channels", token, token_type, channel_payload)
    else:
        create_permission_overwrite(guild_id, token, token_type, channel["id"], guild_id, "role", 0, 1024)
        create_permission_overwrite(guild_id, token, token_type, channel["id"], role_id, "role", 1024, 0)

    return {"category": category, "channel": channel, "role": role}


def main() -> int:
    parser = argparse.ArgumentParser(description="Create a private Discord beta tester category and channel")
    parser.add_argument("--token", required=True, help="Discord bot token or user token")
    parser.add_argument("--guild-id", required=True, help="Discord guild/server ID")
    parser.add_argument("--category-name", default="Beta Testers", help="Name of the category")
    parser.add_argument("--channel-name", default="beta-testers", help="Name of the text channel")
    parser.add_argument("--role-name", default="Beta Tester", help="Role name to grant access")
    parser.add_argument("--token-type", default="bot", choices=["bot", "user"], help="Authorization type")
    args = parser.parse_args()

    if not args.token:
        print("A Discord token is required", file=sys.stderr)
        return 2

    try:
        result = ensure_private_channel(args.guild_id, args.token, args.token_type, args.category_name, args.channel_name, args.role_name)
    except Exception as exc:  # pragma: no cover - CLI entrypoint
        print(str(exc), file=sys.stderr)
        return 1

    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
