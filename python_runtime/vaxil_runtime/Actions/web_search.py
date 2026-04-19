from __future__ import annotations

import json
import os
import urllib.parse
import urllib.request

from .common import failure, success

SPEC = {
    "name": "web_search",
    "title": "Web Search",
    "description": "Search the web using Brave or DuckDuckGo fallback.",
    "args_schema": {"type": "object", "properties": {"query": {"type": "string"}}, "required": ["query"]},
    "risk_level": "network_access",
    "platforms": ["windows", "linux"],
    "supports_background": True,
    "tags": ["web"],
}


def _compact_sources(sources: list[dict[str, str]], limit: int = 8) -> str:
    lines: list[str] = []
    for index, source in enumerate(sources[:limit], start=1):
        title = (source.get("title") or "untitled").strip()
        url = (source.get("url") or "").strip()
        snippet = (source.get("snippet") or "").strip()
        if not url:
            continue
        lines.append(f"[{index}] {title} | {url} | {snippet[:260]}")
    return "\n".join(lines)


def _brave_sources(body: str) -> list[dict[str, str]]:
    try:
        data = json.loads(body)
    except Exception:
        return []
    results = ((data.get("web") or {}).get("results") or [])
    sources: list[dict[str, str]] = []
    for row in results:
        if not isinstance(row, dict):
            continue
        url = str(row.get("url") or "").strip()
        if not url:
            continue
        sources.append(
            {
                "title": str(row.get("title") or "").strip(),
                "url": url,
                "snippet": str(row.get("description") or "").strip(),
            }
        )
    return sources


def _duck_sources(body: str) -> list[dict[str, str]]:
    try:
        data = json.loads(body)
    except Exception:
        return []
    sources: list[dict[str, str]] = []
    abstract_url = str(data.get("AbstractURL") or "").strip()
    if abstract_url:
        sources.append(
            {
                "title": str(data.get("Heading") or "").strip(),
                "url": abstract_url,
                "snippet": str(data.get("AbstractText") or "").strip(),
            }
        )

    def walk_topics(topics) -> None:
        for topic in topics or []:
            if not isinstance(topic, dict):
                continue
            if isinstance(topic.get("Topics"), list):
                walk_topics(topic.get("Topics"))
                continue
            url = str(topic.get("FirstURL") or "").strip()
            text = str(topic.get("Text") or "").strip()
            if url:
                sources.append({"title": text[:100], "url": url, "snippet": text})

    walk_topics(data.get("RelatedTopics"))
    return sources


def _search_success(summary: str, detail: str, query: str, provider: str, body: str, sources: list[dict[str, str]]) -> dict:
    content = _compact_sources(sources)
    return success(
        summary,
        detail,
        query=query,
        provider=provider,
        sources=sources,
        content=content,
        text=content,
        json=body,
    )


def run(_service, args, context):
    query = str(args.get("query") or "").strip()
    if not query:
        return failure("Search failed", "A query is required.")
    brave_key = str(context.get("braveSearchApiKey") or os.environ.get("BRAVE_SEARCH_API_KEY") or "").strip()
    if brave_key:
        url = f"https://api.search.brave.com/res/v1/web/search?q={urllib.parse.quote(query)}"
        request = urllib.request.Request(url, headers={"Accept": "application/json", "X-Subscription-Token": brave_key, "User-Agent": "VaxilPythonRuntime/1.0"})
        with urllib.request.urlopen(request, timeout=20) as response:
            body = response.read().decode("utf-8", errors="replace")
        sources = _brave_sources(body)
        return _search_success("Search complete", "Fetched Brave Search results.", query, "brave", body, sources)
    fallback_url = f"https://api.duckduckgo.com/?q={urllib.parse.quote(query)}&format=json&no_html=1&skip_disambig=1"
    request = urllib.request.Request(fallback_url, headers={"User-Agent": "VaxilPythonRuntime/1.0"})
    with urllib.request.urlopen(request, timeout=20) as response:
        body = response.read().decode("utf-8", errors="replace")
    sources = _duck_sources(body)
    return _search_success("Search complete", "Fetched DuckDuckGo fallback results.", query, "duckduckgo", body, sources)
