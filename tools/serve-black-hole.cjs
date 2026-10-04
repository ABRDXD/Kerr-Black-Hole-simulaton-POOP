// Dependency-free loopback server for the built browser renderer.
const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "../web-demo/dist");
const port = Number(process.argv[2] || 4173);
if (!Number.isInteger(port) || port < 1024 || port > 65535) throw new Error("Port must be 1024–65535.");
if (!fs.existsSync(path.join(root, "index.html"))) throw new Error("Build web-demo first, or run start-black-hole.ps1 -Build.");
const mime = { ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8", ".json": "application/json", ".png": "image/png", ".svg": "image/svg+xml", ".ico": "image/x-icon" };
const server = http.createServer((req, res) => {
  if (req.method !== "GET" && req.method !== "HEAD") { res.writeHead(405, { Allow: "GET, HEAD" }).end(); return; }
  let pathname;
  try { pathname = decodeURIComponent(new URL(req.url, "http://localhost").pathname); }
  catch { res.writeHead(400).end("Invalid path"); return; }
  const file = path.resolve(root, "." + (pathname === "/" ? "/index.html" : pathname));
  const relative = path.relative(root, file);
  if (relative.startsWith("..") || path.isAbsolute(relative)) { res.writeHead(403).end("Forbidden"); return; }
  fs.stat(file, (error, stat) => {
    if (error || !stat.isFile()) { res.writeHead(404).end("Not found"); return; }
    res.writeHead(200, { "Content-Type": mime[path.extname(file)] || "application/octet-stream", "Content-Length": stat.size, "Cache-Control": "no-cache", "X-Content-Type-Options": "nosniff" });
    if (req.method === "HEAD") { res.end(); return; }
    const stream = fs.createReadStream(file);
    stream.on("error", () => res.destroy());
    stream.pipe(res);
  });
});
server.on("error", (error) => {
  console.error(error.code === "EADDRINUSE" ? `Port ${port} is already in use. Use -Port with another port, or open the running demo.` : error.message);
  process.exitCode = 1;
});
server.listen(port, "127.0.0.1", () => console.log(`KerrLab is ready at http://127.0.0.1:${port}/\nKeep this terminal open. Ctrl+C stops the server.`));
