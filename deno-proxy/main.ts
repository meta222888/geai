const GEMINI_API_KEY = Deno.env.get("GEMINI_API_KEY") ?? "";
const GEMINI_BASE = "https://generativelanguage.googleapis.com";

function corsHeaders() {
  return {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
    "Access-Control-Allow-Headers": "content-type, authorization, x-goog-api-key",
  };
}

Deno.serve(async (req: Request) => {
  if (req.method === "OPTIONS") {
    return new Response(null, { headers: corsHeaders() });
  }

  const url = new URL(req.url);
  if (url.pathname === "/" || url.pathname === "/health") {
    return Response.json({ ok: true, name: "Geai Gemini Proxy" }, { headers: corsHeaders() });
  }

  if (!GEMINI_API_KEY) {
    return Response.json({ error: "Missing GEMINI_API_KEY" }, { status: 500, headers: corsHeaders() });
  }

  if (!url.pathname.startsWith("/v1beta/")) {
    return Response.json({ error: "Only /v1beta/* is supported" }, { status: 404, headers: corsHeaders() });
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

  const outHeaders = new Headers(resp.headers);
  for (const [k, v] of Object.entries(corsHeaders())) outHeaders.set(k, v);

  return new Response(resp.body, {
    status: resp.status,
    statusText: resp.statusText,
    headers: outHeaders,
  });
});
