import { resolve } from "node:path";
import { defineConfig } from "vite";

const rootDir = resolve(__dirname, "..");
const examplesDir = resolve(rootDir, "examples");

export default defineConfig({
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
    fs: {
      allow: [rootDir]
    }
  }
});
