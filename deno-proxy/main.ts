const GEMINI_API_KEY = Deno.env.get("GEMINI_API_KEY") ?? "";
const GEMINI_BASE = "https://generativelanguage.googleapis.com";

function corsHeaders() {
  return {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
    "Access-Control-Allow-Headers": "content-type, authorization, x-goog-api-key",
  };
}

function json(data: unknown, init: ResponseInit = {}) {
  const headers = new Headers(init.headers);
  for (const [k, v] of Object.entries(corsHeaders())) headers.set(k, v);
  headers.set("content-type", "application/json; charset=utf-8");
  return Response.json(data, { ...init, headers });
}

function text(data: string, init: ResponseInit = {}) {
  const headers = new Headers(init.headers);
  for (const [k, v] of Object.entries(corsHeaders())) headers.set(k, v);
  headers.set("content-type", "text/plain; charset=utf-8");
  return new Response(data, { ...init, headers });
}

function extractGeminiText(data: any): string {
  const parts = data?.candidates?.[0]?.content?.parts;
  if (Array.isArray(parts)) {
    const out = parts.map((p) => p?.text ?? "").filter(Boolean).join("\n").trim();
    if (out) return out;
  }
  const msg = data?.error?.message;
  if (typeof msg === "string" && msg.length > 0) return `Error: ${msg}`;
  return JSON.stringify(data, null, 2);
}

Deno.serve(async (req: Request) => {
  if (req.method === "OPTIONS") {
    return new Response(null, { headers: corsHeaders() });
  }

  const url = new URL(req.url);
  if (url.pathname === "/" || url.pathname === "/health") {
    return json({
      ok: true,
      name: "Geai Gemini Proxy",
      hasGeminiApiKey: GEMINI_API_KEY.length > 0,
      geminiApiKeyLength: GEMINI_API_KEY.length,
    });
  }

  if (!GEMINI_API_KEY) {
    return json({
      error: "Missing GEMINI_API_KEY",
      hint: "Set GEMINI_API_KEY in Deno Deploy Environment Variables, then redeploy the app.",
    }, { status: 500 });
  }

  if (!url.pathname.startsWith("/v1beta/")) {
    return json({ error: "Only /v1beta/* is supported" }, { status: 404 });
  }

  const target = new URL(GEMINI_BASE + url.pathname + url.search);
  target.searchParams.set("key", GEMINI_API_KEY);

  const headers = new Headers(req.headers);
  headers.delete("host");
  headers.delete("authorization");
  headers.delete("x-goog-api-key");
  headers.set("content-type", headers.get("content-type") ?? "application/json");

  const resp = await fetch(target, {
    method: req.method,
    headers,
    body: req.method === "GET" || req.method === "HEAD" ? undefined : await req.arrayBuffer(),
  });

  const raw = await resp.text();
  try {
    const data = JSON.parse(raw);
    return text(extractGeminiText(data), {
      status: resp.ok ? 200 : resp.status,
      statusText: resp.statusText,
    });
  } catch {
    return text(raw, {
      status: resp.status,
      statusText: resp.statusText,
    });
  }
});
