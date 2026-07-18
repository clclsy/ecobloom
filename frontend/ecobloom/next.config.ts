import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  // Static export -> outputs to /out, deployable directly to Cloudflare Pages
  // (or any static host) with zero server runtime.
  output: "export",
  images: {
    unoptimized: true,
  },
};

export default nextConfig;
