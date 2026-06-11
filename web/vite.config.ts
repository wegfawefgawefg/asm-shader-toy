import { execSync } from "node:child_process";
import { resolve } from "node:path";
import { defineConfig } from "vite";
import packageJson from "./package.json" with { type: "json" };

const rootDir = resolve(__dirname, "..");
const examplesDir = resolve(rootDir, "examples");
const gitCommit =
  process.env.GITHUB_SHA?.slice(0, 7) ??
  (() => {
    try {
      return execSync("git rev-parse --short HEAD", { cwd: rootDir, stdio: ["ignore", "pipe", "ignore"] }).toString().trim();
    } catch {
      return "dev";
    }
  })();

export default defineConfig({
  base: process.env.VITE_BASE ?? "/",
  define: {
    __APP_VERSION__: JSON.stringify(`${packageJson.version}-${gitCommit}`)
  },
  plugins: [
    {
      name: "asm-shader-toy-example-reload",
      configureServer(server) {
        server.watcher.add(resolve(examplesDir, "**/*.{asm,inc}"));
        server.watcher.on("change", (file) => {
          if (file.startsWith(examplesDir)) {
            server.ws.send({ type: "full-reload" });
          }
        });
      }
    }
  ],
  server: {
    host: "0.0.0.0",
    port: 5173,
    strictPort: true,
    fs: {
      allow: [rootDir]
    }
  }
});
